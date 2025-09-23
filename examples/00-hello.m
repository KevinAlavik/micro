/* Macro programming languge - Hello, World */
int printf(...); // for now, later we will import a library

int main()
{
    int test = 69;
    printf("Hello, %s! %d\n", "World",
           test); // UB - since printf isnt really defined somewhere, will compile
                  // with warning. You can fix this by foward declaring printf like:
                  //  int printf(...);
    return 0;
}
