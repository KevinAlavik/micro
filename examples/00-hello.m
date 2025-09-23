/*
Macro Programming languge
=========================
0. Hello World
*/

int add(int a, int b)
{
    return a + b;
}

int main(int argc)
{
    int test = 123;
    test     = "whaaat, this is invalid!";
    printf("This is a test\n");

    if (test == 123)
    {
        printf("this is correct!\n");
    }
    else if (test == 456)
    {
        printf("something went super wrong...\n");
    }
    else
    {
        printf("we cooked\n");
    }

    return add(1, 1);
}
