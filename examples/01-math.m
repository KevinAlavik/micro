/* Macro programming languge - math */
int add(int a, int b)
{
    return a + b;
}

int printf(...);
int main()
{
    printf("add(1, 1) = %ld\n", add(1, 1));
    return 0;
}
