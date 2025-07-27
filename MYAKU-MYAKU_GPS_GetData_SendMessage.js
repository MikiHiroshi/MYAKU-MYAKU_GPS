// ----------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------

// Google Spreadsheet ID
const SPREADSHEET_ID = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; 

// Discord Webhook URL
const DISCORD_WEBHOOK_URL = 'https://discord.com/api/webhooks/xxxxxxxxxxxxxx/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx';


// Sheet names
const GPS_LOG_SHEET_NAME = 'gps_log';
const REGION_SHEET_NAME = 'area_list';


// ----------------------------------------------------------------
// Main Processing
// ----------------------------------------------------------------


// Main function when a POST request is received


function doPost(e) {
  // Check if this log is recorded
  Logger.log('--- doPost function started ---'); 

  try {
    Logger.log('doPost received a request.');

    // Safely check for the existence of e.postData and e.postData.contents
    if (!e || !e.postData || typeof e.postData.contents === 'undefined') {
      // Error log if the request has no content
      Logger.log('Error: Request received, but it contains no postData or contents.');
      // Log the entire received event object 'e' for debugging
      Logger.log('Received event object (e): ' + JSON.stringify(e));
      // Return an error to the client
      return ContentService
        .createTextOutput(JSON.stringify({status: 'error', message: 'Request body is missing or empty.'}))
        .setMimeType(ContentService.MimeType.JSON);
    }

    // Log the received raw data for verification
    const rawData = e.postData.contents;
    Logger.log('Raw POST data: ' + rawData);
    
    // Get JSON data from the request
    const data = JSON.parse(rawData);
    Logger.log('Request data: ' + JSON.stringify(data));

    const newLatitude = parseFloat(data.latitude);
    const newLongitude = parseFloat(data.longitude);

    if (isNaN(newLatitude) || isNaN(newLongitude)) {
      throw new Error('Invalid latitude or longitude.');
    }

    // Get the spreadsheet and sheets
    const spreadsheet = SpreadsheetApp.openById(SPREADSHEET_ID);
    const gpsLogSheet = spreadsheet.getSheetByName(GPS_LOG_SHEET_NAME);
    
    // --- Insert new data into the second row ---
    // Record Latitude in column D, Longitude in column E
    const rowData = [
      new Date(),       // Column A: Current time
      data.timestamp,   // Column B: Timestamp
      data.distance,    // Column C: Distance
      newLatitude,      // Column D: Latitude
      newLongitude,     // Column E: Longitude
      data.altitude     // Column F: Altitude
    ];
    gpsLogSheet.insertRowBefore(2);
    gpsLogSheet.getRange(2, 1, 1, rowData.length).setValues([rowData]);
    Logger.log('New GPS data inserted. Lat in D, Lon in E.');

    // --- Location check and Discord notification ---
    checkLocationAndNotify(spreadsheet, newLatitude, newLongitude);

    // Success response
    return ContentService
      .createTextOutput(JSON.stringify({status: 'success', message: 'Data recorded and checked successfully'}))
      .setMimeType(ContentService.MimeType.JSON);
      
  } catch (error) {
    // Log the error
    Logger.log('Error in doPost: ' + error.toString());
    Logger.log('Stack: ' + error.stack);
    // Error response
    return ContentService
      .createTextOutput(JSON.stringify({status: 'error', message: error.toString()}))
      .setMimeType(ContentService.MimeType.JSON);
  }
}


 //* Checks location information and notifies Discord if within an area.
 //* @param {GoogleAppsScript.Spreadsheet.Spreadsheet} spreadsheet - The target spreadsheet.
 //* @param {number | string} currentLat - Current latitude.
 //* @param {number | string} currentLon - Current longitude.
 
function checkLocationAndNotify(spreadsheet, currentLat, currentLon) {
  currentLat = parseFloat(currentLat);
  currentLon = parseFloat(currentLon);

  Logger.log(`Checking location: lat=${currentLat}, lon=${currentLon}`);
  const regionSheet = spreadsheet.getSheetByName(REGION_SHEET_NAME);
  
  if (regionSheet.getLastRow() <= 1) {
    Logger.log('No region data found in sheet: ' + REGION_SHEET_NAME);
    return;
  }

  // Get data from column D to K (D:Region Name, E:Latitude, F:Longitude, G:Latitude Error, H:Longitude Error, I:Last Sent Timestamp, J:Grace Period, K:Send Count)
  const regions = regionSheet.getRange(2, 4, regionSheet.getLastRow() - 1, 8).getValues();

  for (let i = 0; i < regions.length; i++) {
    const region = regions[i];
    const rowIndex = i + 2; // Actual row number on the spreadsheet

    const regionName = region[0]; // Column D
    const centerLat = parseFloat(region[1]);  // Column E
    const centerLon = parseFloat(region[2]);  // Column F
    const latError = parseFloat(region[3]);  // Column G
    const lonError = parseFloat(region[4]);  // Column H
    const lastSentTimestamp = region[5];      // Column I
    const gracePeriodStr = region[6];         // Column J
    const sendCount = parseInt(region[7], 10); // Column K

    if (!regionName || isNaN(centerLat) || isNaN(centerLon) || isNaN(latError) || isNaN(lonError)) {
      Logger.log(`Skipping row ${rowIndex} due to incomplete or invalid geo data.`);
      continue;
    }

    const latMin = centerLat - latError;
    const latMax = centerLat + latError;
    const lonMin = centerLon - lonError;
    const lonMax = centerLon + lonError;

    const isLatInRange = currentLat >= latMin && currentLat <= latMax;
    const isLonInRange = currentLon >= lonMin && currentLon <= lonMax;

    Logger.log(`--- Checking region: ${regionName} (Row: ${rowIndex}) ---`);
    Logger.log(`Area match: ${isLatInRange && isLonInRange}`);

    if (isLatInRange && isLonInRange) {
      // --- Condition 2: Check grace period ---
      let isTimeOK = false;
      if (!lastSentTimestamp || !gracePeriodStr) {
        isTimeOK = true; // If no timestamp or grace period is set, it's unconditionally OK
        Logger.log('Time condition is OK (no time data).');
      } else {
        try {
          const lastSentTime = new Date(lastSentTimestamp);
          const gracePeriodParts = gracePeriodStr.toString().split(':');
          const graceHours = parseInt(gracePeriodParts[0], 10) || 0;
          const graceMinutes = parseInt(gracePeriodParts[1], 10) || 0;
          const gracePeriodMillis = (graceHours * 3600 + graceMinutes * 60) * 1000;
          
          const nextSendableTime = new Date(lastSentTime.getTime() + gracePeriodMillis);
          const currentTime = new Date();
          
          isTimeOK = currentTime > nextSendableTime;
          Logger.log(`Time Check: Current=${currentTime}, Next Sendable=${nextSendableTime}, OK=${isTimeOK}`);
        } catch(e) {
          Logger.log(`Error parsing time for row ${rowIndex}: ${e.toString()}`);
          isTimeOK = true; // Allow if parsing error occurs
        }
      }

      // --- Condition 3: Check send count ---
      const isCountOK = !isNaN(sendCount) && sendCount > 0;
      Logger.log(`Count Check: Send count=${sendCount}, OK=${isCountOK}`);

      if (isTimeOK && isCountOK) {
        const message = `Here is ${regionName}.`;
        Logger.log(`All conditions met for "${regionName}". Sending notification.`);
        sendToDiscord(message);

        // Update last sent timestamp in Column I (9th column)
        regionSheet.getRange(rowIndex, 9).setValue(new Date());
        // Decrement send count in Column K (11th column)
        regionSheet.getRange(rowIndex, 11).setValue(sendCount - 1);
        
        Logger.log(`Updated row ${rowIndex}: Last sent time and count.`);
      } else {
        Logger.log(`Skipping notification for "${regionName}" due to time or count restrictions.`);
      }
    }
  }
}


// * Sends a message to Discord.
// * @param {string} message - The message to send.

function sendToDiscord(message) {
  if (DISCORD_WEBHOOK_URL.includes('xxxxxxxx/yyyyyyyy')) {
    const warning = '【CONFIGURATION REQUIRED】Discord Webhook URL is still at its default value. Please update it to the correct URL in the script editor.';
    Logger.log(warning);
    return; 
  }
  
  try {
    const payload = {
      content: message
    };

    const options = {
      method: 'post',
      contentType: 'application/json',
      payload: JSON.stringify(payload)
    };

    Logger.log('Sending to Discord...');
    const response = UrlFetchApp.fetch(DISCORD_WEBHOOK_URL, options);
    Logger.log('Discord response code: ' + response.getResponseCode());
  } catch(error) {
    Logger.log('Error sending to Discord: ' + error.toString());
  }
}


// * Test GET function.

function doGet(e) {
  return ContentService
    .createTextOutput('GPS Tracker Webhook is running. Use testNotification() for debugging.')
    .setMimeType(ContentService.MimeType.TEXT);
}

// ----------------------------------------------------------------
// Debugging Functions
// ----------------------------------------------------------------

// * Tests notifications using the latest GPS data from the spreadsheet.
// * You can run this function from the GAS editor to verify its operation.

function testNotification() {
  try {
    const spreadsheet = SpreadsheetApp.openById(SPREADSHEET_ID);
    const gpsLogSheet = spreadsheet.getSheetByName(GPS_LOG_SHEET_NAME);
    
    if (gpsLogSheet.getLastRow() < 2) {
      Logger.log('No test data in Sheet 1. Please enter test latitude and longitude in row 2.');
      return;
    }
    
    // Get Latitude (Column D) and Longitude (Column E) from the second row (latest data) of Sheet 1
    const latestData = gpsLogSheet.getRange("D2:E2").getValues()[0];
    const lat = latestData[0];
    const lon = latestData[1];

    if (typeof lat !== 'number' || typeof lon !== 'number') {
        Logger.log('Please enter valid numerical latitude and longitude in columns D and E of row 2 in Sheet 1.');
        return;
    }

    Logger.log('--- Running testNotification ---');
    checkLocationAndNotify(spreadsheet, lat, lon);
    Logger.log('--- Finished testNotification ---');

  } catch(error) {
    Logger.log('Error in testNotification: ' + error.toString());
  }
}
