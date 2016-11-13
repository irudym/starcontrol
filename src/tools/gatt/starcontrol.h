//
// Created by Igor Rudym on 31/10/2016.
//
/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
        *
        *     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" CTSIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
        * limitations under the License.
*/

#ifndef STARCONTROL_STARCONTROL_H
#define STARCONTROL_STARCONTROL_H

#define DEVICE_NAME		"Stardust LED"
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static int brightness_value;
static int scheduler_onoff = 0;
static int start_time = 0x1800;
static int end_time = 0x1800;

static int64_t seconds_before_sheduler_update = 0;
extern void starcontrol_init(void);
extern void check_scheduler(void);


#endif //STARCONTROL_STARCONTROL_H
