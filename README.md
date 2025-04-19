Device for Arlington animal shelter to keep track dog feed time, etc.

![image](https://github.com/user-attachments/assets/f280f908-aaa5-4a76-998f-9f02889430af)


Two devices always sync data (time) using radio communication.
When any device power up (starts) it will sync (ask) other device to sync time.


Advanced:
To enter deviece menu hold first (top) button for 10+ seconds.

![image](https://github.com/user-attachments/assets/07e4916b-fa50-491b-83e6-4bd023b98e36)


Navigation:

* Button 1: Enter/exit menu (hold 10+ sec to enter menu)
* Button 2: Up
* Button 3: Down
* Button 4: Increase selected value
* Button 5: Decrease selected value


Menu settings:
* Yel: 2 - set hours to light up yellow LED
* Red: 4 - set hours to light up red LED
* ID: 1/0 - communication ID. For main device ID should be 0, for other device ID is 1. Devices will not be able to communicate if IDs will be identical in both devices.
* Pow: 0 - radio transmitter power:
   - 0: -18 dBm (weakest)
   - 1: -12 dBm
   - 2: -18 dBm
   - 3: -6 dBm (strongest)
* Debug information
   - S: 1 - Debug packages sent (during menu is opened)
   - R: 1 - Debug packages received (during menu is opened)
   - Dif: 1 - Debug packages lost (to measure communication quality). Increase Pow setting (up to 3) to increase transmitter power for better quality.
   - Ret: 1 - Amount of retries to synchronize time with other device
 
Troubleshooting/Debug messages displayed:
* Sync 0 - (on device start) initial time synchronization. Hides after 1 sec. If label do not hides after 5-7 sec this indicates issues in communication between devices:
   - Distance between devices is too big and radio transmitter power is not enough. Try increase "Pow" setting in device menu to 3.
   - Check ID in both devices (in menu). One device should have ID: 0, other device ID: 1.
   - Devices are too close to each other (less than 1 feet), in this case sometimes they are not able to communicate too.
* Sync... - (after pressing a button) synchronization between devices. Hides after 1 sec. If not - read previous item.
* Device is stuck (not reacting on buttons and time doesn't updates). This problem was fixed, but this can happen when powering device using Type-C + mobile charger. There should be no issues by connecting Type-c to a computer via USB.
