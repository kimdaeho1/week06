#include <stdio.h>
#include <arpa/inet.h>

int main()
{
    const char  *ip_address = "107.212.96.29";    //dotted decimal 형식의 IP주소를 문자열로 저장
    struct in_addr addr;                            //IPv4 주소를 저장하기 위한 구조체 내부에 32비트 정수형 변수인 s_addr이 정의되어 있고, 변환된 IP주소가 이 변수에 저장됨.
    if (inet_pton (AF_INET, ip_address, &addr) != 1)// IP주소를 dotted decimal 형식의 문자열에서 네트워크 바이트 순서로 변환하는 함수.
    {                                               //IPv4 주소체계를 사용한다는 것을 지정, IP주소, 구조체의 주소(변환될 IP주소가 저장됨)
        printf("Invalid IP address\n");
        return 1;
    }

    printf("0x%x\n", ntohl(addr.s_addr));

    return 0;
}
