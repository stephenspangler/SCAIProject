# Starcraft: Brood War AI Project for CIS 365  
  
Instructions  
Clone the repository to a directory of your choosing.  
Navigate to the cloned directory.  
Copy bwapi-data (the entire folder) to your Starcraft directory.  

Launch Chaoslauncher.exe from the Chaoslauncher directory. Select BWAPI 4.1.2 Injector [DEBUG] or BWAPI 4.1.2 Injector [RELEASE depending on whether you're launching a debug or release DLL. Hit "Config" and on the 6th line, change:  
"ai = ..." to "ai = path/to/your/clone/release/TerranAIModule.dll"   
and  
"ai_dbg = ..." to "ai_dbg = path/to/your/clone/debug/TerranAIModule.dll". 
  
External library credit:  
BWAPI - https://github.com/bwapi/bwapi  
