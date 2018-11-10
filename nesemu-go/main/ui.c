#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>

#include <string.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

#include "../components/ugui/ugui.h"
#include "../components/odroid/odroid_display.h"
#include "../components/odroid/odroid_input.h"


UG_GUI gui;
uint16_t *fb;

static void pset(UG_S16 x, UG_S16 y, UG_COLOR color)
{
    fb[y * 320 + x] = color;
}

static void window1callback(UG_MESSAGE *msg)
{
}

static void UpdateDisplay()
{
    UG_Update();
    ili9341_write_frame_rectangleLE(0, 0, 320, 240, fb);
}

// A utility function to swap two elements
inline static void swap(char **a, char **b)
{
    char *t = *a;
    *a = *b;
    *b = t;
}

static int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++)
    {
        int d = tolower((int)*a) - tolower((int)*b);
        if (d != 0 || !*a)
            return d;
    }
}

//------
/* This function takes last element as pivot, places
   the pivot element at its correct position in sorted
    array, and places all smaller (smaller than pivot)
   to left of pivot and all greater elements to right
   of pivot */
static int partition(char *arr[], int low, int high)
{
    char *pivot = arr[high]; // pivot
    int i = (low - 1);       // Index of smaller element

    for (int j = low; j <= high - 1; j++)
    {
        // If current element is smaller than or
        // equal to pivot
        if (strcicmp(arr[j], pivot) < 0) //(arr[j] <= pivot)
        {
            i++; // increment index of smaller element
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return (i + 1);
}

/* The main function that implements QuickSort
 arr[] --> Array to be sorted,
  low  --> Starting index,
  high  --> Ending index */
static void quickSort(char *arr[], int low, int high)
{
    if (low < high)
    {
        /* pi is partitioning index, arr[p] is now
           at right place */
        int pi = partition(arr, low, high);

        // Separately sort elements before
        // partition and after partition
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

IRAM_ATTR static void bubble_sort(char **files, int count)
{
    int n = count;
    bool swapped = true;

    while (n > 0)
    {
        int newn = 0;
        for (int i = 1; i < n; ++i)
        {
            if (strcicmp(files[i - 1], files[i]) > 0)
            {
                char *temp = files[i - 1];
                files[i - 1] = files[i];
                files[i] = temp;

                newn = i;
            }
        } //end for
        n = newn;
    } //until n = 0
}

static void SortFiles(char **files, int count)
{
    int n = count;
    bool swapped = true;

    if (count > 1)
    {
        //quickSort(files, 0, count - 1);
        bubble_sort(files, count - 1);
    }
}

static int GetFiles(const char *path, const char *extension, char ***filesOut)
{
    //printf("GetFiles: path='%s', extension='%s'\n", path, extension);
    //OpenSDCard();

    const int MAX_FILES = 4096;

    int count = 0;
    char **result = (char **)heap_caps_malloc(MAX_FILES * sizeof(void *), MALLOC_CAP_SPIRAM);
    //char** result = (char**)malloc(MAX_FILES * sizeof(void*));
    if (!result)
        abort();

    //*filesOut = result;

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        printf("opendir failed.\n");
        abort();
    }

    int extensionLength = strlen(extension);
    if (extensionLength < 1)
        abort();

    char *temp = (char *)malloc(extensionLength + 1);
    if (!temp)
        abort();

    memset(temp, 0, extensionLength + 1);

    // Print files
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        //printf("File: %s\n", entry->d_name);
        size_t len = strlen(entry->d_name);

        bool skip = false;

        // ignore 'hidden' files (MAC)
        if (entry->d_name[0] == '.')
            skip = true;

        // ignore BIOS file(s)
        char *lowercase = (char *)malloc(len + 1);
        if (!lowercase)
            abort();

        lowercase[len] = 0;
        for (int i = 0; i < len; ++i)
        {
            lowercase[i] = tolower((int)entry->d_name[i]);
        }
        if (strcmp(lowercase, "bios.col") == 0)
            skip = true;

        free(lowercase);

        memset(temp, 0, extensionLength + 1);
        if (!skip)
        {
            for (int i = 0; i < extensionLength; ++i)
            {
                temp[i] = tolower((int)entry->d_name[len - extensionLength + i]);
            }

            if (len > extensionLength)
            {
                if (strcmp(temp, extension) == 0)
                {
                    result[count] = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM);
                    //result[count] = (char*)malloc(len + 1);
                    if (!result[count])
                    {
                        abort();
                    }

                    strcpy(result[count], entry->d_name);
                    ++count;

                    if (count >= MAX_FILES)
                        break;
                }
            }
        }
    }

    closedir(dir);
    free(temp);
    //CloseSDCard();

    SortFiles(result, count);

#if 0
    for (int i = 0; i < count; ++i)
    {
        printf("GetFiles: %d='%s'\n", i, result[i]);
    }
#endif

    *filesOut = result;
    return count;
}

static void FreeFiles(char **files, int count)
{
    for (int i = 0; i < count; ++i)
    {
        free(files[i]);
    }

    free(files);
}

#define MAX_OBJECTS 20
#define ITEM_COUNT 10

UG_WINDOW window1;
UG_BUTTON button1;
UG_TEXTBOX textbox[ITEM_COUNT];
UG_OBJECT objbuffwnd1[MAX_OBJECTS];

static void DrawPage(char **files, int fileCount, int currentItem)
{
    static const size_t MAX_DISPLAY_LENGTH = 62; //38;

    int page = currentItem / ITEM_COUNT;
    page *= ITEM_COUNT;

    // Reset all text boxes
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        uint16_t id = TXB_ID_0 + i;
        //UG_TextboxSetForeColor(&window1, id, C_BLACK);
        UG_TextboxSetText(&window1, id, "");
    }

    if (fileCount < 1)
    {
        const char *text = "(empty)";

        uint16_t id = TXB_ID_0 + (ITEM_COUNT / 2);
        UG_TextboxSetText(&window1, id, (char *)text);

        UpdateDisplay();
    }
    else
    {
        char *displayStrings[ITEM_COUNT];
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            displayStrings[i] = NULL;
        }

        for (int line = 0; line < ITEM_COUNT; ++line)
        {
            if (page + line >= fileCount)
                break;

            uint16_t id = TXB_ID_0 + line;

            if ((page) + line == currentItem)
            {
                UG_TextboxSetForeColor(&window1, id, C_BLACK);
                UG_TextboxSetBackColor(&window1, id, C_YELLOW);
            }
            else
            {
                UG_TextboxSetForeColor(&window1, id, C_BLACK);
                UG_TextboxSetBackColor(&window1, id, C_WHITE);
            }

            char *fileName = files[page + line];
            if (!fileName)
                abort();

            size_t fileNameLength = strlen(fileName) - 4; // remove extension
            size_t displayLength = (fileNameLength <= MAX_DISPLAY_LENGTH) ? fileNameLength : MAX_DISPLAY_LENGTH;

            displayStrings[line] = (char *)heap_caps_malloc(displayLength + 1, MALLOC_CAP_SPIRAM);
            if (!displayStrings[line])
                abort();

            strncpy(displayStrings[line], fileName, displayLength);
            displayStrings[line][displayLength] = 0; // NULL terminate

            UG_TextboxSetText(&window1, id, displayStrings[line]);
        }

        UpdateDisplay();

        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            free(displayStrings[i]);
        }
    }
}

const char* ui_choosefile(const char* path, const char* extension, const char* current)
{
    const char *result = NULL;

    fb = (uint16_t *)heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM);
    if (!fb) abort();


    UG_Init(&gui, pset, 320, 240);

    UG_WindowCreate(&window1, objbuffwnd1, MAX_OBJECTS, window1callback);

    UG_WindowSetTitleText(&window1, "SELECT A FILE");
    UG_WindowSetTitleTextFont(&window1, &FONT_10X16);
    UG_WindowSetTitleTextAlignment(&window1, ALIGN_CENTER);

    UG_S16 innerWidth = UG_WindowGetInnerWidth(&window1);
    UG_S16 innerHeight = UG_WindowGetInnerHeight(&window1);
    UG_S16 titleHeight = UG_WindowGetTitleHeight(&window1);
    UG_S16 textHeight = (innerHeight) / ITEM_COUNT;

    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        uint16_t id = TXB_ID_0 + i;
        UG_S16 top = i * textHeight;
        UG_TextboxCreate(&window1, &textbox[i], id, 0, top, innerWidth, top + textHeight - 1);
        UG_TextboxSetFont(&window1, id, &FONT_5X12);
        UG_TextboxSetForeColor(&window1, id, C_BLACK);
        UG_TextboxSetAlignment(&window1, id, ALIGN_CENTER);
        //UG_TextboxSetText(&window1, id, "ABCDEFGHabcdefg");
    }

    UG_WindowShow(&window1);
    UpdateDisplay();

    char **files;
    int fileCount = GetFiles(path, extension, &files);

    // Selection
    int currentItem = 0;

    if (current && (strlen(current) > strlen(path)))
    {
        const char *filename = current + strlen(current);
        while (filename > current)
        {
            if (*filename == '/')
            {
                filename++;
                break;
            }

            --filename;
        }

        printf("%s: searching for '%s'\n", __func__, filename);

        // find the current file
        for (int i = 0; i < fileCount; ++i)
        {
            if (strcmp(files[i], filename) == 0)
            {
                printf("%s: found '%s' at %d\n", __func__, files[i], i);
                currentItem = i;
                break;
            }
        }
    }

    DrawPage(files, fileCount, currentItem);

    odroid_gamepad_state previousState;
    odroid_input_gamepad_read(&previousState);

    while (true)
    {
        odroid_gamepad_state state;
        odroid_input_gamepad_read(&state);

        int page = currentItem / 10;
        page *= 10;

        if (fileCount > 0)
        {
            if (!previousState.values[ODROID_INPUT_DOWN] && state.values[ODROID_INPUT_DOWN])
            {
                if (fileCount > 0)
                {
                    if (currentItem + 1 < fileCount)
                    {
                        ++currentItem;
                        DrawPage(files, fileCount, currentItem);
                    }
                    else
                    {
                        currentItem = 0;
                        DrawPage(files, fileCount, currentItem);
                    }
                }
            }
            else if (!previousState.values[ODROID_INPUT_UP] && state.values[ODROID_INPUT_UP])
            {
                if (fileCount > 0)
                {
                    if (currentItem > 0)
                    {
                        --currentItem;
                        DrawPage(files, fileCount, currentItem);
                    }
                    else
                    {
                        currentItem = fileCount - 1;
                        DrawPage(files, fileCount, currentItem);
                    }
                }
            }
            else if (!previousState.values[ODROID_INPUT_RIGHT] && state.values[ODROID_INPUT_RIGHT])
            {
                if (fileCount > 0)
                {
                    if (page + 10 < fileCount)
                    {
                        currentItem = page + 10;
                        DrawPage(files, fileCount, currentItem);
                    }
                    else
                    {
                        currentItem = 0;
                        DrawPage(files, fileCount, currentItem);
                    }
                }
            }
            else if (!previousState.values[ODROID_INPUT_LEFT] && state.values[ODROID_INPUT_LEFT])
            {
                if (fileCount > 0)
                {
                    if (page - 10 >= 0)
                    {
                        currentItem = page - 10;
                        DrawPage(files, fileCount, currentItem);
                    }
                    else
                    {
                        currentItem = page;
                        while (currentItem + 10 < fileCount)
                        {
                            currentItem += 10;
                        }

                        DrawPage(files, fileCount, currentItem);
                    }
                }
            }
            else if (!previousState.values[ODROID_INPUT_A] && state.values[ODROID_INPUT_A])
            {
                size_t fullPathLength = strlen(path) + 1 + strlen(files[currentItem]) + 1;

                //char* fullPath = (char*)heap_caps_malloc(fullPathLength, MALLOC_CAP_SPIRAM);
                char *fullPath = (char *)malloc(fullPathLength);
                if (!fullPath)
                    abort();

                strcpy(fullPath, path);
                strcat(fullPath, "/");
                strcat(fullPath, files[currentItem]);

                result = fullPath;
                break;
            }
            else if (!previousState.values[ODROID_INPUT_B] && state.values[ODROID_INPUT_B])
            {
                result = NULL;
                break;
            }
        }

        previousState = state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    FreeFiles(files, fileCount);

    free(fb);
    return result;
}
