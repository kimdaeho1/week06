#include <stdio.h>
#include <arpa/inet.h>

int main()
{
    uint32_t hex_ip = 0x0A010140;
    struct in_addr addr;                //네트워크 바이트 순서의 IP주소를 저장할 구조체
    char ip_str[INET_ADDRSTRLEN];       //변환될 dotted decimal 주소를 저장할 버퍼

    addr.s_addr = htonl(hex_ip);        //16진수 주소를 네트워크 바이트 순서로 변환

    if(inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN) == NULL)
    {
        printf("error\n");
        return 1;
    }

    printf("%s\n", ip_str);
    return 0;

}
