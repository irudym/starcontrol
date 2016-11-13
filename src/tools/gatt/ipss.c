/** @file
 *  @brief IP Support Service sample
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <net/ip_buf.h>
#include <net/net_core.h>
#include <net/net_socket.h>

#define DEVICE_NAME		"Test IPSP node"
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)
#define UNKNOWN_APPEARANCE	0x0000

/* The 2001:db8::/32 is the private address space for documentation RFC 3849 */
#define MY_IPADDR { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			0, 0x2 } } }

/* admin-local, dynamically allocated multicast address */
#define MCAST_IPADDR { { { 0xff, 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
			   0x2 } } }

#define UDP_PORT		4242

#if !defined(CONFIG_BLUETOOTH_GATT_DYNAMIC_DB)
static ssize_t read_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	const char *name = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, name,
				 strlen(name));
}

static ssize_t read_appearance(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	uint16_t appearance = sys_cpu_to_le16(UNKNOWN_APPEARANCE);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &appearance,
				 sizeof(appearance));
}

static ssize_t read_model(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t read_manuf(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}
#endif /* CONFIG_BLUETOOTH_GATT_DYNAMIC_DB */

static struct bt_gatt_attr attrs[] = {
#if !defined(CONFIG_BLUETOOTH_GATT_DYNAMIC_DB)
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GAP),
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_DEVICE_NAME, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_GAP_DEVICE_NAME, BT_GATT_PERM_READ,
			   read_name, NULL, DEVICE_NAME),
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_APPEARANCE, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_GAP_APPEARANCE, BT_GATT_PERM_READ,
			   read_appearance, NULL, NULL),
	/* Device Information Service Declaration */
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER, BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_DIS_MODEL_NUMBER, BT_GATT_PERM_READ,
			   read_model, NULL, CONFIG_SOC),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
			       BT_GATT_CHRC_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_DIS_MANUFACTURER_NAME, BT_GATT_PERM_READ,
			   read_manuf, NULL, "Manufacturer"),
#endif /* CONFIG_BLUETOOTH_GATT_DYNAMIC_DB */
	/* IP Support Service Declaration */
	BT_GATT_PRIMARY_SERVICE(BT_UUID_IPSS),
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x20, 0x18),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static inline void reverse(unsigned char *buf, int len)
{
	int i, last = len - 1;

	for (i = 0; i < len / 2; i++) {
		unsigned char tmp = buf[i];

		buf[i] = buf[last - i];
		buf[last - i] = tmp;
	}
}

static inline struct net_buf *prepare_reply(const char *type,
					    struct net_buf *buf)
{
	printk("%s: received %d bytes\n", type, ip_buf_appdatalen(buf));

	/* In this test we reverse the received bytes.
	 * We could just pass the data back as is but
	 * this way it is possible to see how the app
	 * can manipulate the received data.
	 */
	reverse(ip_buf_appdata(buf), ip_buf_appdatalen(buf));

	return buf;
}

static inline bool get_context(struct net_context **recv,
			       struct net_context **mcast_recv)
{
	static struct net_addr mcast_addr;
	static struct net_addr any_addr;
	static struct net_addr my_addr;
	static const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
	static const struct in6_addr in6addr_mcast = MCAST_IPADDR;
	static struct in6_addr in6addr_my = MY_IPADDR;

	mcast_addr.in6_addr = in6addr_mcast;
	mcast_addr.family = AF_INET6;

	any_addr.in6_addr = in6addr_any;
	any_addr.family = AF_INET6;

	my_addr.in6_addr = in6addr_my;
	my_addr.family = AF_INET6;

	*recv = net_context_get(IPPROTO_UDP,
				&any_addr, 0,
				&my_addr, UDP_PORT);
	if (!*recv) {
		printk("%s: Cannot get network context\n", __func__);
		return NULL;
	}

	*mcast_recv = net_context_get(IPPROTO_UDP,
				      &any_addr, 0,
				      &mcast_addr, UDP_PORT);
	if (!*mcast_recv) {
		printk("%s: Cannot get receiving mcast network context\n",
		      __func__);
		return false;
	}

	return true;
}

static inline void receive_and_reply(struct net_context *recv,
				     struct net_context *mcast_recv)
{
	struct net_buf *buf;

	buf = net_receive(recv, TICKS_UNLIMITED);
	if (buf) {
		prepare_reply("unicast ", buf);

		if (net_reply(recv, buf)) {
			ip_buf_unref(buf);
		}
		return;
	}

	buf = net_receive(mcast_recv, TICKS_UNLIMITED);
	if (buf) {
		prepare_reply("multicast ", buf);

		if (net_reply(mcast_recv, buf)) {
			ip_buf_unref(buf);
		}
		return;
	}
}

void ipss_init(void)
{
	bt_gatt_register(attrs, ARRAY_SIZE(attrs));

	bt_conn_cb_register(&conn_callbacks);
}

int ipss_advertise(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		return err;
	}

	return 0;
}

void ipss_listen(void)
{
	static struct net_context *recv;
	static struct net_context *mcast_recv;

	if (!get_context(&recv, &mcast_recv)) {
		printk("%s: Cannot get network contexts\n", __func__);
		return;
	}

	while (1) {
		receive_and_reply(recv, mcast_recv);
	}
}
