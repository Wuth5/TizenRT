#include <stdio.h>
#include <tinyara/config.h>
#include <tinyara/wifi/rtk/wifi_structures.h>
#include <tinyara/wifi/rtk/wifi_constants.h>

#define IP_NAT 1



extern void example_nat_repeater(void);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int wifinat_main(int argc, char *argv[])
#endif
{
	printf("WIFI NAT\n");
	example_nat_repeater();
}