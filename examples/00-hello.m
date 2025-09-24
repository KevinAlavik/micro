/* Macro programming languge - Hello, World */
int printf(...); // for now, later we will import a library

int main()
{
    printf("Hello, World!\n"); // UB - since printf isnt really defined somewhere, will compile
                               // with warning. You can fix this by foward declaring printf like:
                               //  int printf(...);
    return 0;
}
