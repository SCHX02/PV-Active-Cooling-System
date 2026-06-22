function doPost(e){
  try{
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

    var data = JSON.parse(e.postData.contents);

    // --- If sheet is empty, add header ---
    if (sheet.getLastRow() === 0){
      sheet.appendRow([
        "Timestamp","Solar Irradiance (W/m2)","Uncooled Voltage (V)","Uncooled Current(mA)","Uncooled Power (mW)","Uncooled Top Temp(°C)","Uncooled Mid Temp(°C)","Uncooled Bot Temp(°C)","Uncooled Avg Temp(°C)","Cooled Voltage(V)","Cooled Current(mA)","Cooled Power(mW)","Cooled Top Temp(°C)","Cooled Mid Temp(°C)","Cooled Bot Temp(°C)","Cooled Avg Temp(°C)","Pump Voltage(V)",
        "Pump Current(mA)","Pump Power(mW)","Battery Voltage(V)" ,"Battery Current (mA)","Battery Power (mW)", "Water Pump"
      ]);
    }

    // --- Append Real Data ---
    sheet.appendRow([
      new Date(),data.irr, data.uV,data.uC,data.uP,data.uTtemp,data.uMtemp,data.uBtemp,data.uAtemp,
      data.cV,data.cC,data.cP,data.cTtemp,data.cMtemp,data.cBtemp,data.cAtemp,
      data.pV,data.pC,data.pP,
      data.bV, data.bC, data.bP,
      data.pump
    ]);

    return ContentService.createTextOutput("Success");
    } catch(f){
      return ContentService.createTextOutput("Error :" + f.toString());

  }
}
