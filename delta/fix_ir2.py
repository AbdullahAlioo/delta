import re

with open("IRSignal.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# 1. navStack/navTop/currentIsCustom
nav_regex = re.compile(r"    static NavFrame navStack\[MAX_NAV_DEPTH\];\s*static int8_t\s*navTop = -1;\s*static bool\s*currentIsCustom\s*= false;", re.MULTILINE | re.DOTALL)

nav_replacement = """    static NavFrame navStack[MAX_NAV_DEPTH];
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

content, n1 = nav_regex.subn(nav_replacement, content)
print("Replaced navStack:", n1 > 0)

# 2. pushFrame
push_regex = re.compile(r"(static void pushFrame\([^)]*\)\s*\{\s*if\s*\(\s*navTop\s*\+\s*1\s*<\s*MAX_NAV_DEPTH\s*\)\s*\{\s*navTop\+\+;\s*navStack\[navTop\]\s*=\s*\{\s*screen,\s*company,\s*device,\s*task\s*\};)\s*(\})", re.MULTILINE | re.DOTALL)
content, n2 = push_regex.subn(r"\1\n            updateMenuCache();\n        \2", content)
print("Replaced pushFrame:", n2 > 0)

# 3. menuEnter
menu_enter_regex = re.compile(r"void menuEnter\(\)\s*\{\s*navTop = -1;\s*pushFrame\(Screen::RootList\);\s*currentIsCustom\s*=\s*false;\s*currentSignalIndex\s*=\s*0;\s*signalSentFlag\s*=\s*false;\s*\}", re.MULTILINE | re.DOTALL)
menu_enter_repl = """    void menuEnter() {
        navTop = -1;
        currentIsCustom    = false;
        currentSignalIndex = 0;
        signalSentFlag     = false;
        pushFrame(Screen::RootList);
    }"""
content, n3 = menu_enter_regex.subn(menu_enter_repl, content)
print("Replaced menuEnter:", n3 > 0)

# 4. menuGetTitle
menu_get_title_regex = re.compile(r"String menuGetTitle\(\)\s*\{[^\}]+return String\(\);\s*\}", re.MULTILINE | re.DOTALL)
menu_get_title_repl = """    String menuGetTitle() {
        return cachedTitle;
    }"""
content, n4 = menu_get_title_regex.subn(menu_get_title_repl, content)
print("Replaced menuGetTitle:", n4 > 0)

# 5. menuGetCurrentSignalName
menu_get_curr_sig_regex = re.compile(r"String menuGetCurrentSignalName\(\)\s*\{[^\}]+return getFilteredSignalName[^\}]+\}", re.MULTILINE | re.DOTALL)
menu_get_curr_sig_repl = """    String menuGetCurrentSignalName() {
        if (cachedItemNames.size() > 0) return cachedItemNames[0];
        return String();
    }"""
content, n5 = menu_get_curr_sig_regex.subn(menu_get_curr_sig_repl, content)
print("Replaced menuGetCurrentSignalName:", n5 > 0)

# 6. menuGetItemCount
menu_get_item_count_regex = re.compile(r"uint8_t menuGetItemCount\(\)\s*\{[^\}]+case Screen::SignalReady:\s*return 1;\s*\}\s*return 0;\s*\}", re.MULTILINE | re.DOTALL)
menu_get_item_count_repl = """    uint8_t menuGetItemCount() {
        return cachedItemNames.size();
    }"""
content, n6 = menu_get_item_count_regex.subn(menu_get_item_count_repl, content)
print("Replaced menuGetItemCount:", n6 > 0)

# 7. menuGetItemName
menu_get_item_name_regex = re.compile(r"String menuGetItemName\(uint8_t index\)\s*\{[^\}]+case Screen::SignalReady:\s*return menuGetCurrentSignalName\(\);\s*\}\s*return String\(\);\s*\}", re.MULTILINE | re.DOTALL)
menu_get_item_name_repl = """    String menuGetItemName(uint8_t index) {
        if (index < cachedItemNames.size()) return cachedItemNames[index];
        return String();
    }"""
content, n7 = menu_get_item_name_regex.subn(menu_get_item_name_repl, content)
print("Replaced menuGetItemName:", n7 > 0)

# 8. menuBack
menu_back_regex = re.compile(r"void menuBack\(\)\s*\{\s*if\s*\(signalSentFlag\)\s*\{\s*//[^}]*signalSentFlag\s*=\s*false;\s*return;\s*\}\s*if\s*\(navTop\s*>\s*0\)\s*\{\s*navTop--;\s*\}\s*\}", re.MULTILINE | re.DOTALL)
menu_back_repl = """    void menuBack() {
        if (signalSentFlag) {
            signalSentFlag = false;
            updateMenuCache();
            return;
        }
        if (navTop > 0) {
            navTop--;
            updateMenuCache();
        }
    }"""
content, n8 = menu_back_regex.subn(menu_back_repl, content)
print("Replaced menuBack:", n8 > 0)

# 9. acceptSave
accept_save_regex = re.compile(r"currentMode = Mode::Idle;\s*return true;\s*\}", re.MULTILINE | re.DOTALL)
# Actually, this is dangerous if there are multiple. Let's do string replacement for this one
accept_save_old = """        Serial.println("IR save complete: " + name);
        currentMode = Mode::Idle;
        return true;
    }"""
accept_save_new = """        Serial.println("IR save complete: " + name);
        currentMode = Mode::Idle;
        updateMenuCache();
        return true;
    }"""
content = content.replace(accept_save_old, accept_save_new)
print("Replaced acceptSave:", accept_save_new in content)

# 10. deleteSignal
del_sig_old = """        Serial.print("IR deleted signal: ");
        Serial.println(deletedName);
        return true;
    }"""
del_sig_new = """        Serial.print("IR deleted signal: ");
        Serial.println(deletedName);
        updateMenuCache();
        return true;
    }"""
content = content.replace(del_sig_old, del_sig_new)
print("Replaced deleteSignal:", del_sig_new in content)

with open("IRSignal.cpp", "w", encoding="utf-8") as f:
    f.write(content)
