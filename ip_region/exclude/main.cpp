#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdlib.h>  // atoi
#include <arpa/inet.h> // inet_addr
#include <math.h> //pow

#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

using namespace std;

#define OUT_OF_RANGE  1

// trim from start
static inline std::string &ltrim(std::string &s) 
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) 
{
    return ltrim(rtrim(s));
}


struct ip_range
{
    unsigned int start;
    unsigned int end;
};

struct region_info
{
    string name;
    vector<ip_range> regions;
};

vector<region_info> g_region;


static inline const char* get_ip_string(unsigned int ip)
{
    ip = htonl(ip);
    struct in_addr addr;
    memcpy(&addr,&ip,4);;
    return inet_ntoa(addr);
}

int exclude(ip_range big, ip_range small, vector<ip_range> &ret)
{
    if (big.start > small.start || big.end < small.end)
    {
        // cout << "error !" << get_ip_string(big.start) << endl;
        return OUT_OF_RANGE;
    }

    ip_range block[2]; // left and right

    block[0].start = big.start;
    block[0].end   = small.start - 1;

    block[1].start = small.end + 1;
    block[1].end  = big.end;

    for (int i = 0; i < 2; ++i)
    {
        if (block[i].start < block[i].end)
        {
            ret.push_back(block[i]);
        }
    }

    return 0;
}

ip_range get_ip_range(string line)
{
    //   10.0.0.1/30; => 10.0.0.0 - 10.0.0.3 
    ip_range ret;
    size_t pos = line.find("/");
    int mask   = atoi(line.substr(pos + 1, line.length() - pos - 1/*;*/).c_str());

    unsigned int start = ntohl(inet_addr(line.substr(0, pos).c_str()));
    start = start & (0xffffffff << (32 - mask));
    unsigned int end   = 0;

    if(mask >= 0 && mask <= 32)
    {
        end = start | ~(0xffffffff << (32 - mask));
    }
    else
    {
        cout << "wrong line " << line << endl;
        exit(0);
    }

    ret.start = start;
    ret.end   = end;
    return ret;
}

vector<ip_range> get_exclue(const char* file)
{
    vector<ip_range> ret;

    ifstream ifs(file);
    if (!ifs.is_open())
    {
        return ret;
    }
    string line;
    while(getline(ifs, line)) 
    {
        line = trim(line);
        size_t pos = line.find("/");
        if (pos != string::npos)
        {
            ret.push_back(get_ip_range(line));
        }
    }

    return ret;
}

int init_whole_region(const char* file)
{
    ifstream ifs(file);
    if (!ifs.is_open())
    {
        cout << "open file " << file << " failed" << endl;
        return 1;
    }

    string line;
    size_t pos;
    const char* name_key = "region";

    region_info local_region;

    while(getline(ifs, line)) 
    {
        line = trim(line);
        pos = line.find("//");
        if (pos != string::npos)
        {
            if (pos == 0)
            {
                continue;
            }
            else
            {
                line = line.substr(0, pos);
            }
        }

        pos = line.find(name_key);
        if (pos != string::npos && pos == 0)
        {
            if (local_region.name != "")
            {
                g_region.push_back(local_region);
            }
            pos = line.find("{");
            if (pos == string::npos)
            {
                cout << "error when parse region name" << endl;
                exit(0);
            }
            string name = line.substr(strlen(name_key), pos - strlen(name_key));
            name = trim(name);
            local_region.name = name;
            local_region.regions.clear();
            continue;
        }
        else
        {
            pos = line.find("/");
            if (pos != string::npos)
            {
                local_region.regions.push_back(get_ip_range(line));
#ifdef CHECK_REGION_ERR                
                string start_ip_str = line.substr(0, pos);
                ip_range tmp = get_ip_range(line);
                if (start_ip_str != get_ip_string(tmp.start))
                {
                    cout << local_region.name << endl;
                    cout << line << "-" << get_ip_string(tmp.start) <<endl;
                }
#endif
            }
        }
    }

#if 0
    for (vector<region_info>::iterator i = g_region.begin(); i != g_region.end(); ++i)
    {
        cout << i->name << endl;
        for (vector<ip_range>::iterator sub_i = i->regions.begin(); sub_i != i->regions.end(); ++sub_i)
        {
            cout << get_ip_string(sub_i->start) << "-" << get_ip_string(sub_i->end) << endl; 
        }
    }
#endif
    return 0;
}

int ip_range_to_mask(ip_range r, vector<string> &regions)
{
    unsigned int start = r.start;
    int i;
    for (i = 0; (i < 31) && (pow(2, i + 1) < r.end - r.start); ++i)
    {
        if ((start >> i) & 0x1)
        {
            break;
        }
    }

    r.start = start + pow(2, i);
    cout << get_ip_string(start) << "/" << 32 - i << endl;
    if (r.start <= r.end)
    {
        ip_range_to_mask(r, regions);
    }

    return 0;
}

void test()
{
    ip_range lhs = get_ip_range("10.0.0.0/26;");
    ip_range rhs = get_ip_range("10.0.0.5/32;");

    vector<ip_range> ret;
    exclude(lhs, rhs, ret);
    for (vector<ip_range>::iterator i = ret.begin(); i != ret.end(); ++i)
    {
        cout << get_ip_string(i->start) << "-" << get_ip_string(i->end) << endl;
        vector<string> depart;
        ip_range_to_mask(*i, depart);
    }
}

void exclude_one_file(const char* file)
{
    vector<ip_range> ex_ranges = get_exclue(file);
    for (vector<region_info>::iterator i = g_region.begin(); i != g_region.end(); ++i)
    {
        for (vector<ip_range>::iterator sub_i = i->regions.begin(); sub_i != i->regions.end(); ++sub_i)
        {
            for (vector<ip_range>::iterator ex_i = ex_ranges.begin(); ex_i != ex_ranges.end(); ++ex_i)
            {
                vector<ip_range> ex;
                if (exclude(*sub_i, *ex_i, ex) == 0)
                {
                    cout << i->name << endl;
                    cout << "region_new range: " << get_ip_string(sub_i->start) << "-" << get_ip_string(sub_i->end) << endl;
                    cout << "exclude range:    " << get_ip_string(ex_i->start) << "-" << get_ip_string(ex_i->end) << endl;
                    cout << "result :" << endl;
                    for (vector<ip_range>::iterator ret_i = ex.begin(); ret_i != ex.end(); ++ret_i)
                    {
                        cout << get_ip_string(ret_i->start) << "-" << get_ip_string(ret_i->end) << " => mask format" << endl;
                        vector<string> depart;
                        ip_range_to_mask(*ret_i, depart);
                    }
                    cout << endl;
                }
            }

        }
    }
}

int main()
{
    int ret = init_whole_region("region_new");
    if (ret) {
        return ret;
    }

    const char* files[] = {"1.txt", "2.txt", "3.txt", "4.txt", "5.txt"};
    for (int i = 0; i < sizeof(files)/sizeof(files[0]); ++i)
    {
        exclude_one_file(files[i]);
    }

    return 0;
}
