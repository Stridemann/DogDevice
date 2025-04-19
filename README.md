Device for Arlington animal shelter to keep track dog feed time, etc.
![image](https://github.com/user-attachments/assets/4655c0de-8315-4550-9265-ffe06d9dcf7f)

Two devices always sync data (time) using radio communication.
When any device power up (starts) it will sync (ask) other device to sync time.


Advanced:
To enter deviece menu hold first (top) button for 10+ seconds.

![image](https://github.com/user-attachments/assets/b855e0c1-3e94-42ea-8105-eff03908c9c0)


Navigation:

* Button 1: Enter/exit menu (hold 10+ sec to enter menu)
* Button 2: Up
* Button 3: Down
* Button 4: Increase selected value
* Button 5: Decrease selected value


Menu settings:
* Yel: 2 - set hours to light up yellow LED
* Red: 4 - set hours to light up red LED
* ID: 1/0 - communication ID. For main device ID should be 0, for other device ID is 1. Devices will not be able to communicate if IDs will be identical in all devices.
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
