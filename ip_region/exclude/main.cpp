#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdlib.h>  // atoi
#include <arpa/inet.h> // inet_addr

using namespace std;

struct region_info
{
    string name;
    vector<vector<ip_range> > regions;
};

struct ip_range
{
    unsigned int start;
    unsigned int end;
};

static inline const char* get_ip_strint(unsigned int ip)
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
        cout << "error !" << get_ip_strint(big.start) << endl;
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
    //   61.135.184.193/26; => 
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

vector<string> get_exclue(const char* file)
{
    vector<string> ret;

    ifstream ifs(file);
    if (!ifs.is_open())
    {
        return ret;
    }
    string line;
    while(getline(ifs, line)) 
    {
        size_t pos = line.find("/");
        if (pos != string::npos)
        {
            ret.push_back(line.substr(pos, line.length()));
            get_ip_range(line);
        }
    }

    return ret;
}

int init_whole_region(const char* file)
{
    
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
        cout << get_ip_strint(i->start) << "-" << get_ip_strint(i->end) << endl;
    }    
}

int main()
{
    test();
    return 0;
}
