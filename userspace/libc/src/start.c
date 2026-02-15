extern int main(int argc, char **argv);
extern void exit(int status);

void _start() {
    /* TODO: Parse argc/argv from stack if kernel puts them there */
    int ret = main(0, 0);
    exit(ret);
}
