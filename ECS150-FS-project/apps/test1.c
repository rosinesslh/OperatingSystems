#include "fs.h"

int main(int argc, char **argv)
{
	fs_mount(argv[1]);
	fs_create(argv[2]);
	fs_umount();
}
