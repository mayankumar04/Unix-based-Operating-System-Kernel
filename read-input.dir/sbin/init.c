#include "libc.h"

// This test tests interrupt driven keyboard input

void readInputUntilKey(int exitKey)
{
    char buffer[1];
    ssize_t bytesRead;

    while (1)
    {
        bytesRead = read(0, buffer, 1);

        if (bytesRead > 0)
        {
            if ((int)*buffer == exitKey)
            {
                break;
            }
            printf("%c", *buffer);
            // printf("%d", (int) *buffer);
        }
        else if (bytesRead < 0)
        {
            printf("Error reading from keyboard\n");
            break;
        }
    }
}

void printBufferContents(const char *buffer, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        printf("%c", buffer[i]);
    }
    printf("\n"); // New line at the end for readability
}

int main()
{
    int fd = tui();
    printf("*** The fd is this: %d\n", fd);
    set_tui(fd);

    printf("*** Reading input until key test\n");
    printf("Type something. Press 'return' to submit.\n");


    readInputUntilKey(27);

    printf("*** Read n characters test\n");
    printf("Type 5 characters\n");
    char buffer[5];
    ssize_t bytesRead;
    bytesRead = read(0, buffer, 5);
    buffer[bytesRead] = 0;
    printf("You typed: ");
    printBufferContents(buffer, bytesRead + 1);
    printf("Exiting Keyboard Test Program\n");
    shutdown();
    return 0;
}