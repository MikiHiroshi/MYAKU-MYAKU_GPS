# MYAKU-MYAKU GPS

## introduction 
MYAKU-MYAKU is the official character and a symbolic mascot of Expo 2025 Osaka, Kansai, Japan.  
When we go to Expo 2025, MYAKU-MYAKU GPS will tell us where we are inside the Expo venue via Discord.  
As a tracker, it also records everywhere we have walked within the venue and shows our path on a map.

<img width="1070" height="595" alt="MYAKU-MYAKU_GPS" src="https://github.com/user-attachments/assets/5168fb65-d265-41f3-afdf-6e1c6e320f70" />


## Build Instructions
This project uses the M5StickC Plus and the GPS Unit v1.1 for M5Stack.  
System components: Google Spreadsheet, Google Apps Script, Ambient, Discord  
Device: Mobile WiFi Router  


<img width="1920" height="1080" alt="system_over_all" src="https://github.com/user-attachments/assets/c816d53b-be8b-4e6c-a7ba-b98ec60e6bdc" />

## Project Details  

Every time the preset travel distance threshold is reached, the device sends the current latitude and longitude to Google Spreadsheet and Google Apps Script.  
When we get close to a pavilion (based on its pre-registered coordinates in the spreadsheet), a message is automatically sent to Discord notifying us of our proximity.

The movement distance threshold can be adjusted by pressing the B button.  
This threshold value is saved using NVS (Non-Volatile Storage), so it is retained even after the device is powered off.  
We can also manually send our location at any time by pressing the A button.

The screen shows:  
- Latitude and longitude  
- Number of satellites detected  
- WiFi connection status  
- Distance traveled  
- Current threshold value for location updates  

The rotation of MYAKU-MYAKU’s eyes indicates that the device is running.

![M5StickCPlus_GPS](https://github.com/user-attachments/assets/c93ad9aa-dd16-4eed-8e23-bc42d092a38b)

## google spreadsheet

Location data is logged to a Google Spreadsheet via Google Apps Script.

<img width="1258" height="359" alt="googlespreadsheet_gps_log" src="https://github.com/user-attachments/assets/bb825b9e-cb46-4aa2-bbaf-8b0eae347bac" />

<img width="1258" height="536" alt="googlespreadsheet_area_list" src="https://github.com/user-attachments/assets/fb0ae4a2-d118-4667-9467-e49180634089" />

## send a message on Discord
When we get close to a pavilion (based on data in the sheet), a message is sent to Discord. 

<img width="340" height="739" alt="Dicord" src="https://github.com/user-attachments/assets/afae3316-f658-47e7-9f3c-20be14b8bc99" />

## tracker in Ambient
The location is also sent to Ambient, which visualizes our entire walking path within the Expo 2025 venue on a map — serving as a personal tracker.

<img width="1015" height="581" alt="ambient_map" src="https://github.com/user-attachments/assets/951117f8-c376-481e-942c-f8e124baa996" />
















