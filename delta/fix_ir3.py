import re

with open("IRSignal.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# 1. Find the updateMenuCache block (starts with `    static std::vector<String> cachedItemNames;`)
cache_regex = re.compile(r"    static std::vector<String> cachedItemNames;\s*static String cachedTitle;\s*static void updateMenuCache\(\) \{.*?(?=    static bool    currentIsCustom    = false;)", re.MULTILINE | re.DOTALL)

match = cache_regex.search(content)
if match:
    cache_code = match.group(0)
    print("Found updateMenuCache!")
    
    # 2. Replace the cache_code at the top with `navStack` declarations
    nav_str = """
    static NavFrame navStack[MAX_NAV_DEPTH];
    static int8_t   navTop = -1;

"""
    content = content.replace(cache_code, nav_str)
    
    # 3. Insert cache_code right above `static void pushFrame(`
    push_frame_str = "    static void pushFrame("
    content = content.replace(push_frame_str, cache_code + "\n" + push_frame_str)
    
    with open("IRSignal.cpp", "w", encoding="utf-8") as f:
        f.write(content)
    print("Done moving updateMenuCache")
else:
    print("Could not find updateMenuCache block")
