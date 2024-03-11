#include <fmt/ranges.h>
#include "simplomon.hh"
#include "sol/sol.hpp"
#include <fmt/chrono.h>
using namespace std;

sol::state g_lua;

bool demo(sol::table data) {
  sol::table b = data["b"];
  sol::object a = data["a"];

  sol::table inner = b.as<sol::table>();

  fmt::print("c={}\n", (int)data["c"]);
  fmt::print("a[1]={}\n", (int)data["a"][1]);
  fmt::print("b[2]={}\n", (string)data["b"][2]);

  a.as<sol::table>().for_each([](std::pair<sol::object, sol::object> kvp) {
    int num = kvp.first.as<int>();
    int n = kvp.second.as<double>();
    fmt::print("{} = {}\n", num, n);
  });
  //  fmt::print("{}\n", lua.get<vector<string>>(data["b"]));
    //  fmt::print("{}\n", b);
  return false;
}

/* every checker has a table of properties, and you get an error if you put unexpected things in there.
   DailyChime{utcHour=11}
   DNSChecker{server="100.25.31.6", domain="berthub.eu", type="A", acceptable={"86.82.68.237", "217.100.190.174"}}
   DNSChecker{server="10.0.0.1", domain="hubertnet.nl", type="MX", acceptable={"5 server.hubertnet.nl.", "10 ziggo.berthub.eu."}, minAlerts=3, alertWindow=180}

   Checkers have mandatory and optional fields. The optional fields can get their default from a global setting
   One of the optional field is who gets alerts

   The bridge between Lua and C++ must be very simple but also robust. 
   Maybe have C++ ingest a Lua table directly? And have it use primitives to check that?
     like getListOfIPAddresses(), getListOfStrings(), getListOfNumbers()
 */

/*
  function that checks if mandatory fields are present, allows optional fields
  and panics over other fields */

/*
a.as<sol::table>().for_each([](std::pair<sol::object, sol::object> kvp) {
 */

void checkLuaTable(sol::table data,
                       const set<string>& mandatory,
                       const set<string>& opt)
{
  set<string> mand = mandatory;
  data.for_each([&](sol::object key, sol::object value) {
    string k = key.as<string>();
    auto iter = mand.find(k);
    if(iter != mand.end()) {
      mand.erase(iter);
      return;
    }
    
    if(!mandatory.count(k) && !opt.count(k))
      throw std::runtime_error(fmt::format("Unknown parameter '{}' passed", k));
  });
    
  if(!mand.empty())
    throw std::runtime_error(fmt::format("Missing mandatory fields '{}'", mand));
  
}

  
class DailyChimeChecker : public Checker
{
public:
  DailyChimeChecker(sol::table data)
  {
    checkLuaTable(data, {"utcHour"});
    d_utcHour = data["utcHour"];
    
  }

  CheckResult perform()
  {
    time_t now = time(nullptr);
    now -= d_utcHour * 3600;
    struct tm tm;
    gmtime_r(&now, &tm);
    return fmt::format("Your daily chime for {:%Y-%m-%d}. This is not an alert", tm);
  }
private:
  int d_utcHour;
  
};


void initLua()
{
  g_lua.open_libraries(sol::lib::base, sol::lib::package);

  g_lua.set_function("dailyChime", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<DailyChimeChecker>(data));
  });

  g_lua.set_function("https", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<HTTPSChecker>(data));
  });
  g_lua.set_function("dnssoa", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<DNSSOAChecker>(data));
  });
  g_lua.set_function("tcpportclosed", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<TCPPortClosedChecker>(data));
  });
  g_lua.set_function("dns", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<DNSChecker>(data));
  });

  g_lua.set_function("httpredir", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<HTTPRedirChecker>(data));
  });

  g_lua.set_function("rrsig", [&](sol::table data) {
    g_checkers.emplace_back(make_unique<RRSIGChecker>(data));
  });

  
  g_lua.set_function("pushoverNotifier", [&](sol::table data) {
    g_reporter = make_unique<PushoverReporter>(data.get<string>("user"),
                                               data.get<string>("apikey"));
  });
  

}
