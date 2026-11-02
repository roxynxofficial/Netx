#include "parser.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <iphlpapi.h>
using socket_t = SOCKET;
static constexpr socket_t bad_socket = INVALID_SOCKET;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
using socket_t = int;
static constexpr socket_t bad_socket = -1;
#endif

namespace fs = std::filesystem;
namespace {
constexpr const char* version = "1.0.0";
const auto start_time = std::chrono::steady_clock::now();
std::string theme = "Cyber";
std::vector<std::string> history;
std::map<std::string, std::string> settings{{"timeout_ms","1000"},{"scan_delay_ms","100"}};
std::string os_name();

const char* accent() {
    if (theme == "Matrix") return "\033[1;38;5;46m";
    if (theme == "Minimal") return "\033[1;37m";
    if (theme == "Classic") return "\033[1;38;5;220m";
    return "\033[1;38;5;51m";
}
const char* brand_color() {
    if (theme == "Matrix") return "\033[1;38;5;82m";
    if (theme == "Minimal") return "\033[1;37m";
    if (theme == "Classic") return "\033[1;38;5;208m";
    return "\033[1;38;5;46m";
}
const char* border_color() {
    if (theme == "Minimal") return "\033[38;5;245m";
    if (theme == "Classic") return "\033[38;5;220m";
    return "\033[38;5;39m";
}
const char* muted_color() { return "\033[38;5;245m"; }
const char* reset_color() { return "\033[0m"; }

void enable_terminal_styles() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}
void render_banner() {
    const auto b = border_color(), g = brand_color(), a = accent(), m = muted_color(), r = reset_color();
    std::cout
        << b << "╭────────────────────────────────────────────────────────────────────╮" << r << '\n'
        << b << "│" << r << "  " << g << "███╗   ██╗███████╗████████╗██╗  ██╗" << r << "  " << a << "NETX" << r << "                         " << b << "│" << r << '\n'
        << b << "│" << r << "  " << g << "████╗  ██║██╔════╝╚══██╔══╝╚██╗██╔╝" << r << "  " << a << "CYBERSECURITY FRAMEWORK" << r << "       " << b << "│" << r << '\n'
        << b << "│" << r << "  " << g << "██╔██╗ ██║█████╗     ██║    ╚███╔╝" << r << "  " << m << "MODULAR SECURITY CONSOLE" << r << "      " << b << "│" << r << '\n'
        << b << "│" << r << "  " << g << "██║╚██╗██║██╔══╝     ██║    ██╔██╗" << r << "  " << b << "───────────────────────────" << r << "  " << b << "│" << r << '\n'
        << b << "│" << r << "  " << g << "██║ ╚████║███████╗   ██║   ██╔╝ ██╗" << r << "  " << m << "VERSION  " << r << a << version << r << "                   " << b << "│" << r << '\n'
        << b << "│" << r << "  " << g << "╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝  ╚═╝" << r << "  " << m << "ENGINE   " << r << a << "C++20" << r << "                  " << b << "│" << r << '\n'
        << b << "╰────────────────────────────────────────────────────────────────────╯" << r << '\n'
        << "  " << g << "● ONLINE" << r << "  " << m << "7 modules · " << os_name() << " · " << theme << " theme" << r << '\n'
        << "  " << m << "Type " << r << a << "help" << r << m << " to explore  ·  " << r << a << "Ctrl+C" << r << m << " to close" << r << "\n\n";
}

void close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
bool valid_host(const std::string& h) {
    if (h.empty() || h.size() > 253 || h.find_first_of(" /\\\t\r\n") != std::string::npos) return false;
    return std::all_of(h.begin(), h.end(), [](unsigned char c){ return std::isalnum(c) || c=='.' || c=='-' || c==':' || c=='%'; });
}
std::string os_name() {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux/Unix";
#endif
}
std::string now() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream o; o << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S"); return o.str();
}
void good(const std::string& s){ std::cout << "  \033[1;38;5;46m✓  OK\033[0m  " << s << '\n'; }
void warn(const std::string& s){ std::cout << "  \033[1;38;5;220m!  NOTE\033[0m " << s << '\n'; }
void error(const std::string& s){ std::cout << "  \033[1;38;5;203m✕  ERROR\033[0m " << s << '\n'; }
std::string json_escape(const std::string& s) {
    std::string o; for(char c:s){ if(c=='"'||c=='\\') o+='\\'; if(c=='\n') o+="\\n"; else if(c!='\r') o+=c; } return o;
}
void report(const std::string& tool,const std::string& target,const std::vector<std::string>& findings) {
    std::error_code ec; fs::create_directories("reports",ec);
    std::string stem = tool + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::ofstream txt(fs::path("reports")/(stem+".txt"));
    txt << "NETX "<<tool<<" report\nTarget: "<<target<<"\nTimestamp: "<<now()<<"\nFindings:\n";
    std::ofstream js(fs::path("reports")/(stem+".json"));
    js << "{\"tool\":\""<<json_escape(tool)<<"\",\"target\":\""<<json_escape(target)<<"\",\"timestamp\":\""<<now()<<"\",\"findings\":[";
    for(size_t i=0;i<findings.size();++i){ txt << "- "<<findings[i]<<'\n'; if(i)js<<','; js<<'"'<<json_escape(findings[i])<<'"'; }
    js << "]}\n"; good("Reports saved under ./reports/ (TXT and JSON)");
}
std::vector<std::string> resolve(const std::string& host, int family=AF_UNSPEC) {
    std::vector<std::string> out;
    if(!valid_host(host)) return out;
    addrinfo hints{}; hints.ai_family=family; hints.ai_socktype=SOCK_STREAM;
    addrinfo* result=nullptr;
    if(getaddrinfo(host.c_str(),nullptr,&hints,&result)==0){
        for(auto* p=result;p;p=p->ai_next){ char buf[NI_MAXHOST]{}; if(getnameinfo(p->ai_addr,static_cast<socklen_t>(p->ai_addrlen),buf,sizeof(buf),nullptr,0,NI_NUMERICHOST)==0 && std::find(out.begin(),out.end(),buf)==out.end()) out.emplace_back(buf); }
        freeaddrinfo(result);
    }
    return out;
}
bool tcp_check(const std::string& host, unsigned short port, int timeout, long* elapsed=nullptr) {
    addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
    addrinfo* list=nullptr; const auto begin=std::chrono::steady_clock::now();
    if(getaddrinfo(host.c_str(),std::to_string(port).c_str(),&hints,&list)!=0) return false;
    bool connected=false;
    for(auto* a=list;a&&!connected;a=a->ai_next){
        socket_t s=socket(a->ai_family,a->ai_socktype,a->ai_protocol); if(s==bad_socket)continue;
#ifdef _WIN32
        u_long mode=1; ioctlsocket(s,FIONBIO,&mode);
#else
        int flags=fcntl(s,F_GETFL,0); fcntl(s,F_SETFL,flags|O_NONBLOCK);
#endif
        int rc=connect(s,a->ai_addr,static_cast<socklen_t>(a->ai_addrlen));
        if(rc!=0){
#ifdef _WIN32
            fd_set w; FD_ZERO(&w); FD_SET(s,&w); timeval tv{timeout/1000,(timeout%1000)*1000}; rc=select(0,nullptr,&w,nullptr,&tv);
#else
            fd_set w; FD_ZERO(&w); FD_SET(s,&w); timeval tv{timeout/1000,(timeout%1000)*1000}; rc=select(s+1,nullptr,&w,nullptr,&tv);
#endif
            if(rc>0){int err=0; socklen_t len=sizeof(err); getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len); connected=(err==0);}
        } else connected=true;
        close_socket(s);
    }
    freeaddrinfo(list);
    if(elapsed)*elapsed=static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count());
    return connected;
}
std::optional<std::string> hash_file(const std::string& path, const wchar_t* alg) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE h=nullptr; BCRYPT_HASH_HANDLE hash=nullptr; DWORD objlen=0,got=0,hashlen=0;
    if(BCryptOpenAlgorithmProvider(&h,alg,nullptr,0)<0)return std::nullopt;
    BCryptGetProperty(h,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objlen),sizeof(objlen),&got,0);
    BCryptGetProperty(h,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hashlen),sizeof(hashlen),&got,0);
    std::vector<UCHAR> obj(objlen), digest(hashlen);
    if(BCryptCreateHash(h,&hash,obj.data(),objlen,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(h,0);return std::nullopt;}
    std::ifstream f(path,std::ios::binary); if(!f){BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(h,0);return std::nullopt;}
    std::array<char,65536> buf{}; while(f){ f.read(buf.data(),buf.size()); auto n=static_cast<ULONG>(f.gcount()); if(n)BCryptHashData(hash,reinterpret_cast<PUCHAR>(buf.data()),n,0); }
    BCryptFinishHash(hash,digest.data(),hashlen,0); BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(h,0);
    std::ostringstream out; out<<std::hex<<std::setfill('0'); for(auto b:digest)out<<std::setw(2)<<static_cast<unsigned>(b); return out.str();
#else
    (void)path;(void)alg; return std::nullopt;
#endif
}
std::optional<std::string> hash_text(const std::string& text) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE h=nullptr; BCRYPT_HASH_HANDLE hash=nullptr; DWORD objlen=0,got=0,hashlen=0;
    if(BCryptOpenAlgorithmProvider(&h,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return std::nullopt;
    if(BCryptGetProperty(h,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objlen),sizeof(objlen),&got,0)<0 ||
       BCryptGetProperty(h,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hashlen),sizeof(hashlen),&got,0)<0){BCryptCloseAlgorithmProvider(h,0);return std::nullopt;}
    std::vector<UCHAR> obj(objlen), digest(hashlen);
    if(BCryptCreateHash(h,&hash,obj.data(),objlen,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(h,0);return std::nullopt;}
    auto status=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),static_cast<ULONG>(text.size()),0);
    if(status>=0)status=BCryptFinishHash(hash,digest.data(),hashlen,0);
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(h,0); if(status<0)return std::nullopt;
    std::ostringstream out; out<<std::hex<<std::setfill('0'); for(auto b:digest)out<<std::setw(2)<<static_cast<unsigned>(b); return out.str();
#else
    (void)text; return std::nullopt;
#endif
}
void show_help(){
 const auto b = border_color(), a = accent(), g = brand_color(), m = muted_color(), r = reset_color();
 std::cout
 << b << "┌─ COMMAND CENTER ────────────────────────────────────────────────────┐" << r << '\n'
 << "  " << a << "help" << r << "                  " << m << "Command guide and usage examples" << r << '\n'
 << "  " << a << "sysinfo   status" << r << "     " << m << "System and session information" << r << '\n'
 << "  " << a << "modules   history" << r << "    " << m << "Loaded tools and recent commands" << r << '\n'
 << "  " << a << "theme NAME" << r << "          " << m << "Cyber · Matrix · Minimal · Classic" << r << '\n'
 << "  " << a << "config   clear   exit" << r << "   " << m << "Settings, screen, and session" << r << '\n'
 << b << "├─ TOOLKIT ───────────────────────────────────────────────────────────┤" << r << '\n'
 << "  " << g << "NETMAP" << r << "   " << m << "scan HOST  ·  ports HOST [--range 1-128]  ·  interfaces" << r << '\n'
 << "  " << g << "WMAP" << r << "     " << m << "audit URL  ·  headers URL  ·  ssl URL  ·  cookies URL" << r << '\n'
 << "  " << g << "NPING" << r << "    " << m << "HOST [--count N]  ·  tcp HOST PORT  ·  dns HOST" << r << '\n'
 << "  " << g << "DNSX" << r << "     " << m << "lookup DOMAIN  ·  mx|ns|txt DOMAIN  ·  reverse IP" << r << '\n'
 << "  " << g << "VULNX" << r << "    " << m << "local  ·  audit [HOST]" << r << "      " << g << "TRACEX" << r << "  " << m << "hash|metadata|logs FILE" << r << '\n'
 << "  " << g << "CRYPTX" << r << "   " << m << "sha256 TEXT  ·  hash FILE  ·  verify FILE HASH  ·  random" << r << '\n'
 << b << "├─ QUICK START ───────────────────────────────────────────────────────┤" << r << '\n'
 << "  " << m << "$ netmap ports 127.0.0.1 --range 1-128" << r << '\n'
 << "  " << m << "$ wmap audit https://example.org" << r << '\n'
 << "  " << m << "$ cryptx sha256 \"hello world\"" << r << '\n'
 << "  " << b << "Only assess systems you own or are authorized to test." << r << '\n'
 << b << "└─────────────────────────────────────────────────────────────────────┘" << r << '\n';
}
void module(const std::vector<std::string>& a){
 if(a.empty())return; const auto& m=a[0];
 if(m=="netmap"){
  if(a.size()<2||a[1]=="help"){std::cout<<"netmap interfaces | scan HOST | hosts SUBNET | ports HOST [--range 1-1024] | services HOST | report\n";return;}
  if(a[1]=="interfaces"){warn("Portable interface enumeration is not available in this build; use sysinfo/network for host details.");return;}
  if(a[1]=="scan"||a[1]=="hosts"||a[1]=="services") { if(a.size()<3||!valid_host(a[2])){error("Provide a valid hostname or IP address.");return;} auto ips=resolve(a[2]); for(auto& ip:ips)good(a[2]+" resolves to "+ip); if(ips.empty())warn("No address records returned."); report("netmap",a[2],ips); return; }
  if(a[1]=="ports") { if(a.size()<3||!valid_host(a[2])){error("Provide a valid host.");return;} int lo=1,hi=128; if(a.size()>=5&&a[3]=="--range"){auto dash=a[4].find('-');try{if(dash==std::string::npos)throw 1;lo=std::stoi(a[4].substr(0,dash));hi=std::stoi(a[4].substr(dash+1));}catch(...){error("Range must be e.g. 1-1024.");return;}} if(lo<1||hi>65535||hi<lo||hi-lo>2048){error("Port range must be within 1-65535 and at most 2048 ports.");return;} std::vector<std::string> f;for(int p=lo;p<=hi;++p){long ms=0;if(tcp_check(a[2],static_cast<unsigned short>(p),1000,&ms)){auto s=a[2]+":"+std::to_string(p)+" open ("+std::to_string(ms)+"ms)";good(s);f.push_back(s);} if((p-lo)%16==0)std::this_thread::sleep_for(std::chrono::milliseconds(100));} report("netmap",a[2],f); return; }
 }
 if(m=="nping"){
  if(a.size()>=2&&a[1]=="dns"){if(a.size()<3){error("Usage: nping dns HOST");return;}auto b=std::chrono::steady_clock::now();auto ips=resolve(a[2]);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-b).count();for(auto& ip:ips)good(ip);good("DNS lookup: "+std::to_string(ms)+" ms");return;}
  if(a.size()>=2&&a[1]=="tcp"){if(a.size()<4){error("Usage: nping tcp HOST PORT");return;}try{int p=std::stoi(a[3]);if(p<1||p>65535){error("Port must be 1-65535.");return;}long ms=0;good(tcp_check(a[2],static_cast<unsigned short>(p),1000,&ms)?"TCP connected in "+std::to_string(ms)+" ms":"TCP connection failed");}catch(...){error("Invalid port.");}return;}
  if(a.size()<2){error("Usage: nping HOST [--count N]");return;} int count=4;for(size_t i=2;i+1<a.size();++i)if(a[i]=="--count")try{count=std::stoi(a[i+1]);}catch(...){count=0;}if(count<1||count>20){error("Count must be 1-20.");return;} std::vector<long> ms;for(int i=0;i<count;++i){long t=0;bool ok=tcp_check(a[1],static_cast<unsigned short>(80),1000,&t)||tcp_check(a[1],static_cast<unsigned short>(443),1000,&t);if(ok){ms.push_back(t);good("TCP reachability in "+std::to_string(t)+" ms");}else warn("No TCP response (ports 80/443); ICMP ping is not implemented.");if(i+1<count)std::this_thread::sleep_for(std::chrono::milliseconds(250));} if(!ms.empty()){long sum=0;for(auto x:ms)sum+=x;good("Successful probes: "+std::to_string(ms.size())+"/"+std::to_string(count)+", average "+std::to_string(sum/static_cast<long>(ms.size()))+" ms");}return;
 }
 if(m=="dnsx") { if(a.size()<3){std::cout<<"dnsx lookup|mx|ns|txt|reverse NAME\n";return;}auto ips=resolve(a[2]);if(a[1]=="reverse"){for(auto& ip:ips)good(ip);if(ips.empty())warn("Reverse lookup unavailable.");}else{for(auto& ip:ips)good((a[1]=="lookup"?"Address":"System resolver address")+std::string(": ")+ip);if(a[1]!="lookup")warn("MX/NS/TXT record querying requires a DNS record library; address resolution only is available.");}return; }
 if(m=="wmap") { if(a.size()<3||a[1]=="help"){std::cout<<"wmap scan|headers|ssl|cookies|audit URL (HTTP checks are limited to currently implemented connectivity; TLS inspection unavailable)\n";return;}std::string u=a[2];auto scheme=u.find("://");if(scheme==std::string::npos||(u.substr(0,scheme)!="http"&&u.substr(0,scheme)!="https")){error("URL must start with http:// or https://");return;}auto hoststart=scheme+3, end=u.find_first_of("/:?#",hoststart);std::string host=u.substr(hoststart,end==std::string::npos?std::string::npos:end-hoststart);if(!valid_host(host)){error("Malformed URL host.");return;}auto ips=resolve(host);std::vector<std::string> f;f.push_back("Resolved addresses: "+std::to_string(ips.size()));for(auto& ip:ips)good(ip);long ms=0;int port=u.substr(0,scheme)=="https"?443:80;bool ok=tcp_check(host,static_cast<unsigned short>(port),1500,&ms);f.push_back(ok?"TCP port "+std::to_string(port)+" reachable":"TCP port "+std::to_string(port)+" unreachable");good(f.back());if(a[1]=="ssl"&&port!=443)warn("URL is not HTTPS.");if(port==443)warn("TLS certificate details are not available in this dependency-free build.");if(a[1]=="cookies")warn("Cookie flags require an HTTP response parser; no response was fetched.");if(a[1]=="headers"||a[1]=="audit"||a[1]=="scan")warn("This command performs safe DNS/TCP connectivity checks only; HTTP headers and redirect analysis are not implemented.");report("wmap",u,f);return; }
 if(m=="tracex"||m=="cryptx"){
  if(a.size()<2){error("Specify a subcommand.");return;}
  if((a[1]=="hash"||a[1]=="verify")&&a.size()>2){std::string path=a[2];std::string expected=a.size()>3?a[3]:"";auto digest=hash_file(path,BCRYPT_SHA256_ALGORITHM);if(!digest){error("File hashing is currently available on Windows builds only.");return;}good("SHA-256 "+*digest);if(a[1]=="verify"){if(expected.empty()){error("Provide expected hash.");return;}good(*digest==expected?"Hash matches":"Hash does not match");}return;}
  if(m=="cryptx"&&a[1]=="sha256"&&a.size()>2){auto digest=hash_text(a[2]);if(digest)good("SHA-256 "+*digest);else error("Text hashing is currently available on Windows builds only.");return;}
  if(m=="cryptx"&&a[1]=="random"){std::array<unsigned char,24>b{};
#ifdef _WIN32
   if(BCryptGenRandom(nullptr,b.data(),static_cast<ULONG>(b.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0){const char*h="0123456789abcdef";std::string s;for(auto x:b){s+=h[x>>4];s+=h[x&15];}good(s);}else error("Secure random provider failed.");
#else
   error("Secure random provider unavailable.");
#endif
   return;}
  if(m=="tracex"&&a[1]=="metadata"&&a.size()>2){std::error_code ec;auto st=fs::status(a[2],ec);if(ec){error("Cannot inspect file.");return;}good("Size: "+std::to_string(fs::file_size(a[2],ec))+" bytes; regular file: "+(fs::is_regular_file(st)?"yes":"no"));return;}
  if(m=="tracex"&&a[1]=="logs"&&a.size()>2){std::ifstream f(a[2]);if(!f){error("Cannot open log file.");return;}std::string line;size_t n=0;while(std::getline(f,line)&&n<1000){std::cout<<line<<'\n';++n;}good("Displayed "+std::to_string(n)+" lines (maximum 1000).");return;}
  std::cout<<m<<" hash FILE | verify FILE HASH | metadata FILE | logs FILE | cryptx random | cryptx sha256 TEXT\n";return;
 }
 if(m=="vulnx"){if(a.size()>1&&a[1]=="local"){good("Local audit: NETX uses no external listeners; detailed OS hardening checks are platform-specific and unavailable.");report("vulnx","local",{"No full local hardening audit performed"});}else{warn("No vulnerability claim made: remote vulnerability probing is intentionally limited to passive connectivity.");report("vulnx",a.size()>2?a[2]:"local",{"Insufficient evidence for vulnerability findings"});}return;}
}
void command(const std::vector<std::string>& a){
 if(a.empty())return;const auto& c=a[0];
 if(c=="help"||c=="?")show_help();
 else if(c=="clear"||c=="cls")std::cout<<"\033[2J\033[H";
 else if(c=="exit"||c=="quit")std::exit(0);
 else if(c=="version")good(std::string("NETX Cybersecurity Framework  v")+version+"  ·  C++20");
 else if(c=="about"){std::cout<<"\n  "<<brand_color()<<"NETX"<<reset_color()<<"  "<<muted_color()<<"Modular diagnostics, security assessment, and forensic utilities."<<reset_color()<<"\n\n";}
 else if(c=="banner")render_banner();
 else if(c=="sysinfo"||c=="status"){
    auto uptime=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start_time).count();
    std::cout<<"\n  "<<border_color()<<"┌─ SESSION STATUS ─────────────────────────────────────┐"<<reset_color()<<'\n'
      <<"  "<<muted_color()<<"│  SYSTEM      "<<reset_color()<<std::left<<std::setw(38)<<os_name()<<border_color()<<"│"<<reset_color()<<'\n'
      <<"  "<<muted_color()<<"│  ENGINE      "<<reset_color()<<std::left<<std::setw(38)<<"C++20 / NETX "+std::string(version)<<border_color()<<"│"<<reset_color()<<'\n'
      <<"  "<<muted_color()<<"│  MODULES     "<<reset_color()<<std::left<<std::setw(38)<<"7 loaded · all core services ready"<<border_color()<<"│"<<reset_color()<<'\n'
      <<"  "<<muted_color()<<"│  UPTIME      "<<reset_color()<<std::left<<std::setw(38)<<(std::to_string(uptime)+" seconds")<<border_color()<<"│"<<reset_color()<<'\n'
      <<"  "<<border_color()<<"└──────────────────────────────────────────────────────┘"<<reset_color()<<"\n";
 }
 else if(c=="network"||c=="interfaces")warn("Use netmap interfaces; interface listing is not implemented in this portable build.");
 else if(c=="modules"){
    std::cout<<"\n  "<<border_color()<<"┌─ MODULE REGISTRY ─────────────────────────────────────┐"<<reset_color()<<'\n';
    const std::vector<std::pair<std::string,std::string>> mods={{"NETMAP","Network discovery"},{"WMAP","Web assessment"},{"NPING","Connectivity diagnostics"},{"VULNX","Vulnerability review"},{"TRACEX","Digital forensics"},{"CRYPTX","Cryptographic utilities"},{"DNSX","DNS analysis"}};
    for(const auto&[name,desc]:mods)std::cout<<"  "<<brand_color()<<"●  "<<std::left<<std::setw(9)<<name<<reset_color()<<muted_color()<<std::setw(40)<<desc<<reset_color()<<border_color()<<"│"<<reset_color()<<'\n';
    std::cout<<"  "<<border_color()<<"└──────────────────────────────────────────────────────┘"<<reset_color()<<"\n";
 }
 else if(c=="history"){if(history.empty())warn("No commands in this session yet.");else{std::cout<<"\n  "<<accent()<<"RECENT COMMANDS"<<reset_color()<<'\n';for(size_t i=0;i<history.size();++i)std::cout<<"  "<<muted_color()<<std::setw(3)<<std::setfill('0')<<i+1<<reset_color()<<"  "<<history[i]<<'\n';std::cout<<std::setfill(' ');}}
 else if(c=="settings"||c=="config"){
    if(a.size()==4&&a[1]=="set"){settings[a[2]]=a[3];good("Updated "+a[2]+" → "+a[3]);}
    else if(a.size()>1&&a[1]=="set")warn("Usage: config set KEY VALUE");
    else{std::cout<<"\n  "<<accent()<<"ACTIVE SETTINGS"<<reset_color()<<'\n';for(auto&[k,v]:settings)std::cout<<"  "<<std::left<<std::setw(20)<<k<<muted_color()<<v<<reset_color()<<'\n';}
 }
 else if(c=="theme"){
    if(a.size()==1)good("Active theme: "+theme);
    else if(a[1]=="Matrix"||a[1]=="Cyber"||a[1]=="Minimal"||a[1]=="Classic"){theme=a[1];good("Theme changed to "+theme);render_banner();}
    else error("Choose Cyber, Matrix, Minimal, or Classic.");
 }
 else if(c=="report")good("Report destination  →  ./reports  (TXT + JSON)");
 else if(c=="update")warn("Automatic updates are disabled; install a verified release manually.");
 else if(c=="netmap"||c=="wmap"||c=="nping"||c=="dnsx"||c=="vulnx"||c=="tracex"||c=="cryptx")module(a);
 else error("Unknown command. Type 'help'.");
}
}
int main(int argc,char**argv){
#ifdef _WIN32
 WSADATA d{}; if(WSAStartup(MAKEWORD(2,2),&d)!=0){std::cerr<<"Winsock initialization failed\n";return 1;}
 SetConsoleTitleW(L"NETX | Cybersecurity Framework");
#endif
 enable_terminal_styles();
 render_banner();
 if(argc>1){std::ostringstream s;for(int i=1;i<argc;++i){if(i>1)s<<' ';s<<argv[i];}auto a=netx::parse_command(s.str());command(a);return 0;}
 std::string line;while(std::cout<<accent()<<"netx"<<muted_color()<<" › "<<reset_color()<<std::flush&&std::getline(std::cin,line)){if(line.empty())continue;history.push_back(line);if(history.size()>100)history.erase(history.begin());auto t=std::chrono::steady_clock::now();command(netx::parse_command(line));auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t).count();if(line!="exit"&&line!="quit")std::cout<<"  "<<brand_color()<<"◆"<<reset_color()<<" "<<muted_color()<<"finished in "<<ms<<" ms"<<reset_color()<<"\n\n";}
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;
}
