import std.io;

void test()
{
    printf("this is in the function block/scope\n");
    {
        printf("this is inside another block\n");
    }
    printf("fucntion again\n");
}

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

    int  a = 0;
    uint b = 1;
    a      = (int) b;

    int* c = &a;

    return add(1, 1);
}
