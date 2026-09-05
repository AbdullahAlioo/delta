import re

with open("IRSignal.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# Normalize newlines
content = content.replace('\r\n', '\n')

old_companyHasDevice = """    static bool companyHasDevice(Company company, DeviceType device) {
        return getFilteredSignalCount(company, device, Task::On)  > 0 ||
               getFilteredSignalCount(company, device, Task::Off) > 0;
    }"""
new_companyHasDevice = """    static bool companyHasDevice(Company company, DeviceType device) {
        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            if (presets[i].company == company && presets[i].device == device) return true;
        }
        File f = LittleFS.open(storagePath, FILE_READ);
        if (f) {
            String fcontent = f.readString();
            f.close();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, fcontent);
            if (!error && doc.is<JsonArray>()) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    if (obj["c"].as<uint8_t>() == (uint8_t)company && obj["d"].as<uint8_t>() == (uint8_t)device) return true;
                }
            }
        }
        return false;
    }"""
content = content.replace(old_companyHasDevice, new_companyHasDevice)
print("Replaced companyHasDevice: ", old_companyHasDevice in content)

navStack_str = """    static NavFrame navStack[MAX_NAV_DEPTH];
    static int8_t   navTop = -1;

    static bool    currentIsCustom    = false;"""

new_cache_str = """    static NavFrame navStack[MAX_NAV_DEPTH];
    static int8_t   navTop = -1;

    static std::vector<String> cachedItemNames;
    static String cachedTitle;

    static void updateMenuCache() {
        cachedItemNames.clear();
        if (navTop < 0) {
            cachedTitle = String("IR");
            return;
        }
        const NavFrame& f = navStack[navTop];
        
        switch (f.screen) {
            case Screen::TopList:      cachedTitle = String("SELECT DEVICE"); break;
            case Screen::DeviceList:   cachedTitle = getCompanyName((uint8_t)f.company) + String(" - AC/TV"); break;
            case Screen::TaskList:     cachedTitle = getCompanyName((uint8_t)f.company) + String(" ") + getDeviceTypeName((uint8_t)f.device); break;
            case Screen::ProtocolList: cachedTitle = getCompanyName((uint8_t)f.company) + String(" ") + getDeviceTypeName((uint8_t)f.device) + String(" ") + getTaskName((uint8_t)f.task); break;
            case Screen::CustomList:   cachedTitle = String("CUSTOM SIGNALS"); break;
            case Screen::SignalReady:  cachedTitle = signalSentFlag ? String("SIGNAL SENT") : String("READY TO SEND"); break;
            default: cachedTitle = String(); break;
        }
        
        bool needsJson = (f.screen == Screen::TaskList || f.screen == Screen::ProtocolList || f.screen == Screen::CustomList || (f.screen == Screen::SignalReady && currentIsCustom));
        
        JsonDocument doc;
        JsonArray arr;
        if (needsJson) {
            File file = LittleFS.open(storagePath, FILE_READ);
            if (file) {
                String fcontent = file.readString();
                file.close();
                DeserializationError error = deserializeJson(doc, fcontent);
                if (!error && doc.is<JsonArray>()) {
                    arr = doc.as<JsonArray>();
                }
            }
        }
        
        switch (f.screen) {
            case Screen::RootList:
                cachedItemNames.push_back(String("RECORD"));
                cachedItemNames.push_back(String("LIST"));
                break;
            case Screen::TopList: {
                uint8_t companyCount = getCompanyCount();
                for (uint8_t i = 0; i < companyCount; i++) cachedItemNames.push_back(getCompanyName(i));
                cachedItemNames.push_back(String("CUSTOM"));
                cachedItemNames.push_back(String("FLOOD OFF"));
                break;
            }
            case Screen::DeviceList:
                cachedItemNames.push_back(getDeviceTypeName(0));
                cachedItemNames.push_back(getDeviceTypeName(1));
                break;
            case Screen::TaskList: {
                for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++) {
                    bool has = false;
                    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                        if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == (Task)t) { has = true; break; }
                    }
                    if (!has && !arr.isNull()) {
                        for (JsonObject obj : arr) {
                            if (signalMatches(obj, f.company, f.device, (Task)t)) { has = true; break; }
                        }
                    }
                    if (has) cachedItemNames.push_back(getTaskName(t));
                }
                break;
            }
            case Screen::ProtocolList: {
                for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                    if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == f.task) {
                        cachedItemNames.push_back(String(presets[i].name));
                    }
                }
                if (!arr.isNull()) {
                    for (JsonObject obj : arr) {
                        if (signalMatches(obj, f.company, f.device, f.task)) {
                            cachedItemNames.push_back(String(obj["name"].as<const char*>()));
                        }
                    }
                }
                break;
            }
            case Screen::CustomList: {
                if (!arr.isNull()) {
                    for (JsonObject obj : arr) {
                        cachedItemNames.push_back(String(obj["name"].as<const char*>()));
                    }
                }
                break;
            }
            case Screen::SignalReady:
                if (currentIsCustom) {
                    if (!arr.isNull() && currentSignalIndex < arr.size()) {
                        cachedItemNames.push_back(String(arr[currentSignalIndex]["name"].as<const char*>()));
                    } else {
                        cachedItemNames.push_back(String("Unknown"));
                    }
                } else {
                    uint8_t seen = 0;
                    String foundName = String("Unknown");
                    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                        if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == f.task) {
                            if (seen == currentSignalIndex) { foundName = String(presets[i].name); break; }
                            seen++;
                        }
                    }
                    if (foundName == "Unknown" && !arr.isNull()) {
                        for (JsonObject obj : arr) {
                            if (signalMatches(obj, f.company, f.device, f.task)) {
                                if (seen == currentSignalIndex) { foundName = String(obj["name"].as<const char*>()); break; }
                                seen++;
                            }
                        }
                    }
                    cachedItemNames.push_back(foundName);
                }
                break;
        }
    }

    static bool    currentIsCustom    = false;"""

content = content.replace(navStack_str, new_cache_str)

old_pushFrame = """    static void pushFrame(Screen screen, Company company = Company::Haier,
                           DeviceType device = DeviceType::AC, Task task = Task::On) {
        if (navTop + 1 < MAX_NAV_DEPTH) {
            navTop++;
            navStack[navTop] = { screen, company, device, task };
        }
    }"""
new_pushFrame = """    static void pushFrame(Screen screen, Company company = Company::Haier,
                           DeviceType device = DeviceType::AC, Task task = Task::On) {
        if (navTop + 1 < MAX_NAV_DEPTH) {
            navTop++;
            navStack[navTop] = { screen, company, device, task };
            updateMenuCache();
        }
    }"""
content = content.replace(old_pushFrame, new_pushFrame)

old_menuEnter = """    void menuEnter() {
        navTop = -1;
        pushFrame(Screen::RootList);
        currentIsCustom    = false;
        currentSignalIndex = 0;
        signalSentFlag     = false;
    }"""
new_menuEnter = """    void menuEnter() {
        navTop = -1;
        currentIsCustom    = false;
        currentSignalIndex = 0;
        signalSentFlag     = false;
        pushFrame(Screen::RootList);
    }"""
content = content.replace(old_menuEnter, new_menuEnter)

old_menuGetTitle = """    String menuGetTitle() {
        if (navTop < 0) return String("IR");
        const NavFrame& f = navStack[navTop];
        switch (f.screen) {
            case Screen::TopList:      return String("SELECT DEVICE");
            case Screen::DeviceList:   return getCompanyName((uint8_t)f.company) + String(" - AC/TV");
            case Screen::TaskList:     return getCompanyName((uint8_t)f.company) + String(" ") +
                                              getDeviceTypeName((uint8_t)f.device);
            case Screen::ProtocolList: return getCompanyName((uint8_t)f.company) + String(" ") +
                                              getDeviceTypeName((uint8_t)f.device) + String(" ") +
                                              getTaskName((uint8_t)f.task);
            case Screen::CustomList:   return String("CUSTOM SIGNALS");
            case Screen::SignalReady:  return signalSentFlag ? String("SIGNAL SENT")
                                                              : String("READY TO SEND");
        }
        return String();
    }"""
new_menuGetTitle = """    String menuGetTitle() {
        return cachedTitle;
    }"""
content = content.replace(old_menuGetTitle, new_menuGetTitle)

old_menuGetCurrentSignalName = """    String menuGetCurrentSignalName() {
        if (currentIsCustom) return getSignalName(currentSignalIndex);
        if (navTop < 0) return String();
        const NavFrame& f = navStack[navTop];
        return getFilteredSignalName(f.company, f.device, f.task, currentSignalIndex);
    }"""
new_menuGetCurrentSignalName = """    String menuGetCurrentSignalName() {
        if (cachedItemNames.size() > 0) return cachedItemNames[0];
        return String();
    }"""
content = content.replace(old_menuGetCurrentSignalName, new_menuGetCurrentSignalName)

old_menuGetItemCount = """    uint8_t menuGetItemCount() {
        if (navTop < 0) return 0;
        const NavFrame& f = navStack[navTop];
        switch (f.screen) {
            case Screen::RootList:     return 2;
            case Screen::TopList:      return getCompanyCount() + 2;
            case Screen::DeviceList:   return (uint8_t)DeviceType::COUNT;
            case Screen::TaskList: {
                uint8_t count = 0;
                for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++)
                    if (taskHasSignal(f.company, f.device, (Task)t)) count++;
                return count;
            }
            case Screen::ProtocolList: return getFilteredSignalCount(f.company, f.device, f.task);
            case Screen::CustomList:   return getSignalCount();
            case Screen::SignalReady:  return 1;
        }
        return 0;
    }"""
new_menuGetItemCount = """    uint8_t menuGetItemCount() {
        return cachedItemNames.size();
    }"""
content = content.replace(old_menuGetItemCount, new_menuGetItemCount)

old_menuGetItemName = """    String menuGetItemName(uint8_t index) {
        if (navTop < 0) return String();
        const NavFrame& f = navStack[navTop];
        switch (f.screen) {
            case Screen::RootList:
                if (index == 0) return String("RECORD");
                if (index == 1) return String("LIST");
                return String();
            case Screen::TopList: {
                uint8_t companyCount = getCompanyCount();
                if (index < companyCount) return getCompanyName(index);
                if (index == companyCount) return String("CUSTOM");
                return String("FLOOD OFF");
            }
            case Screen::DeviceList:   return getDeviceTypeName(index);
            case Screen::TaskList: {
                uint8_t seen = 0;
                for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++) {
                    if (taskHasSignal(f.company, f.device, (Task)t)) {
                        if (seen == index) return getTaskName(t);
                        seen++;
                    }
                }
                return String();
            }
            case Screen::ProtocolList: return getFilteredSignalName(f.company, f.device, f.task, index);
            case Screen::CustomList:   return getSignalName(index);
            case Screen::SignalReady:  return menuGetCurrentSignalName();
        }
        return String();
    }"""
new_menuGetItemName = """    String menuGetItemName(uint8_t index) {
        if (index < cachedItemNames.size()) return cachedItemNames[index];
        return String();
    }"""
content = content.replace(old_menuGetItemName, new_menuGetItemName)

old_menuBack = """    void menuBack() {
        if (signalSentFlag) {
            // Reveal the same ready-to-send page again instead of jumping
            // back up to the protocol/task list.
            signalSentFlag = false;
            return;
        }
        if (navTop > 0) {
            navTop--;
        }
    }"""
new_menuBack = """    void menuBack() {
        if (signalSentFlag) {
            // Reveal the same ready-to-send page again instead of jumping
            // back up to the protocol/task list.
            signalSentFlag = false;
            updateMenuCache();
            return;
        }
        if (navTop > 0) {
            navTop--;
            updateMenuCache();
        }
    }"""
content = content.replace(old_menuBack, new_menuBack)

old_acceptSave = """        Serial.println("IR save complete: " + name);
        currentMode = Mode::Idle;
        return true;
    }"""
new_acceptSave = """        Serial.println("IR save complete: " + name);
        currentMode = Mode::Idle;
        updateMenuCache();
        return true;
    }"""
content = content.replace(old_acceptSave, new_acceptSave)

old_deleteSignal = """        Serial.print("IR deleted signal: ");
        Serial.println(deletedName);
        return true;
    }"""
new_deleteSignal = """        Serial.print("IR deleted signal: ");
        Serial.println(deletedName);
        updateMenuCache();
        return true;
    }"""
content = content.replace(old_deleteSignal, new_deleteSignal)

# Write out the modified content
with open("IRSignal.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("Done replacing.")
