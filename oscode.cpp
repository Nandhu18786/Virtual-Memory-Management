#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_SIZE 10
#define TOTAL_FRAMES 10
#define TOTAL_PAGES 10
#define SECONDARY_MEMORY_SIZE 10 * TOTAL_PAGES

// Structure for page table entry
typedef struct {
    int frameNumber;
    int valid;
    int lastUsed;
} PageTableEntry;

// Global variables
int fifoQueue[TOTAL_FRAMES];
int fifoIndex = 0;
int timeCounter = 0;
int pageHits = 0;
int pageFaults = 0;

PageTableEntry pageTable[TOTAL_PAGES];
int mainMemory[TOTAL_FRAMES * FRAME_SIZE];
int secondaryMemory[SECONDARY_MEMORY_SIZE];
int nextFrame = 0;
char pageReplacementAlgorithm;
int logicalAddressHistory[100];
int accessCount = 0;

GtkWidget *window;
GtkWidget *grid;
GtkWidget *tableTextView;
GtkWidget *memoryTextView;
GtkWidget *secondaryMemoryTextView;
GtkWidget *entry;
GtkWidget *hitLabel;
GtkWidget *faultLabel;
GtkWidget *algorithmComboBox;
GtkWidget *resetButton;

// Function prototypes
void initializePageTable(PageTableEntry pageTable[]);
void loadPageIntoMemory(int pageNumber, PageTableEntry pageTable[], int mainMemory[], int secondaryMemory[], int *nextFrame, char pageReplacementAlgorithm, int *logicalAddressHistory, int accessCount);
void accessMemory(int logicalAddress, PageTableEntry pageTable[], int mainMemory[], int secondaryMemory[], int *nextFrame, char pageReplacementAlgorithm, int *logicalAddressHistory, int *accessCount);
void updateUI();
void on_access_button_clicked(GtkWidget *widget, gpointer data);
void on_algorithm_changed(GtkComboBox *comboBox, gpointer data);
int getReplacementIndex(char algorithm, PageTableEntry pageTable[], int nextFrame, int *logicalAddressHistory, int currentAccess);
void on_reset_button_clicked(GtkWidget *widget, gpointer data);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Page Replacement Algorithms");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scrolledWindow);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(scrolledWindow), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    // Logical address entry
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter logical address");
    gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 1, 1);

    // Access button
    GtkWidget *accessButton = gtk_button_new_with_label("Access Memory");
    gtk_grid_attach(GTK_GRID(grid), accessButton, 1, 0, 1, 1);
    g_signal_connect(accessButton, "clicked", G_CALLBACK(on_access_button_clicked), NULL);

    // Reset button
    resetButton = gtk_button_new_with_label("Reset");
    gtk_grid_attach(GTK_GRID(grid), resetButton, 2, 0, 1, 1);
    g_signal_connect(resetButton, "clicked", G_CALLBACK(on_reset_button_clicked), NULL);

    // Page replacement algorithm dropdown
    algorithmComboBox = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(algorithmComboBox), NULL, "FIFO");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(algorithmComboBox), NULL, "LRU");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(algorithmComboBox), NULL, "Optimal");
    gtk_combo_box_set_active(GTK_COMBO_BOX(algorithmComboBox), 0); // Default to FIFO
    g_signal_connect(algorithmComboBox, "changed", G_CALLBACK(on_algorithm_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), algorithmComboBox, 3, 0, 1, 1);

    // Page table text view
    tableTextView = gtk_text_view_new();
    GtkWidget *tableScrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(tableScrolledWindow), tableTextView);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tableScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(tableScrolledWindow, TRUE);
    gtk_widget_set_vexpand(tableScrolledWindow, TRUE);
    gtk_grid_attach(GTK_GRID(grid), tableScrolledWindow, 0, 1, 4, 1);

    // Main memory text view
    memoryTextView = gtk_text_view_new();
    GtkWidget *memoryScrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(memoryScrolledWindow), memoryTextView);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(memoryScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(memoryScrolledWindow, TRUE);
    gtk_widget_set_vexpand(memoryScrolledWindow, TRUE);
    gtk_grid_attach(GTK_GRID(grid), memoryScrolledWindow, 0, 2, 4, 1);

    // Secondary memory text view
    secondaryMemoryTextView = gtk_text_view_new();
    GtkWidget *secondaryMemoryScrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(secondaryMemoryScrolledWindow), secondaryMemoryTextView);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(secondaryMemoryScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(secondaryMemoryScrolledWindow, TRUE);
    gtk_widget_set_vexpand(secondaryMemoryScrolledWindow, TRUE);
    gtk_grid_attach(GTK_GRID(grid), secondaryMemoryScrolledWindow, 0, 3, 4, 1);

    // Labels for page hits and faults
    hitLabel = gtk_label_new("Page Hits: 0");
    gtk_grid_attach(GTK_GRID(grid), hitLabel, 0, 4, 1, 1);

    faultLabel = gtk_label_new("Page Faults: 0");
    gtk_grid_attach(GTK_GRID(grid), faultLabel, 1, 4, 1, 1);

    initializePageTable(pageTable);
    for (int i = 0; i < SECONDARY_MEMORY_SIZE; i++) {
        secondaryMemory[i] = i;
    }
    pageReplacementAlgorithm = 'F'; // Default to FIFO

    updateUI();

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

void initializePageTable(PageTableEntry pageTable[]) {
    for (int i = 0; i < TOTAL_PAGES; i++) {
        pageTable[i].frameNumber = -1;
        pageTable[i].valid = 0;
        pageTable[i].lastUsed = -1;
    }
}

void loadPageIntoMemory(int pageNumber, PageTableEntry pageTable[], int mainMemory[], int secondaryMemory[], int *nextFrame, char pageReplacementAlgorithm, int *logicalAddressHistory, int accessCount) {
    if (*nextFrame >= TOTAL_FRAMES) {
        int replaceIndex = getReplacementIndex(pageReplacementAlgorithm, pageTable, *nextFrame, logicalAddressHistory, accessCount);
        if (replaceIndex != -1) {
            int frameNumber = pageTable[replaceIndex].frameNumber;
            pageTable[replaceIndex].valid = 0;
            pageTable[pageNumber].frameNumber = frameNumber;
            pageTable[pageNumber].valid = 1;
            pageTable[pageNumber].lastUsed = timeCounter++;

            for (int i = 0; i < FRAME_SIZE; i++) {
                mainMemory[frameNumber * FRAME_SIZE + i] = secondaryMemory[pageNumber * FRAME_SIZE + i];
            }

            for (int i = 0; i < TOTAL_FRAMES; i++) {
                if (fifoQueue[i] == replaceIndex) {
                    fifoQueue[i] = pageNumber;
                    break;
                }
            }
        }
    } else {
        pageTable[pageNumber].frameNumber = *nextFrame;
        pageTable[pageNumber].valid = 1;
        pageTable[pageNumber].lastUsed = timeCounter++;

        for (int i = 0; i < FRAME_SIZE; i++) {
            mainMemory[(*nextFrame) * FRAME_SIZE + i] = secondaryMemory[pageNumber * FRAME_SIZE + i];
        }

        fifoQueue[fifoIndex] = pageNumber;
        fifoIndex = (fifoIndex + 1) % TOTAL_FRAMES;

        (*nextFrame)++;
    }
}

void accessMemory(int logicalAddress, PageTableEntry pageTable[], int mainMemory[], int secondaryMemory[], int *nextFrame, char pageReplacementAlgorithm, int *logicalAddressHistory, int *accessCount) {
    int pageNumber = logicalAddress / FRAME_SIZE;
    int offset = logicalAddress % FRAME_SIZE;

    if (pageNumber >= TOTAL_PAGES) {
        g_print("Invalid logical address. Page number %d is out of bounds.\n", pageNumber);
        return;
    }

    if (pageTable[pageNumber].valid == 0) {
        g_print("Page fault! Page %d is not in memory. Applying page replacement algorithm.\n", pageNumber);
        loadPageIntoMemory(pageNumber, pageTable, mainMemory, secondaryMemory, nextFrame, pageReplacementAlgorithm, logicalAddressHistory, *accessCount);
        pageFaults++;
    } else {
        g_print("Page %d is in memory at frame %d.\n", pageNumber, pageTable[pageNumber].frameNumber);
        pageTable[pageNumber].lastUsed = timeCounter++;
        pageHits++;
    }

    int physicalAddress = pageTable[pageNumber].frameNumber * FRAME_SIZE + offset;
    g_print("Logical address %d maps to physical address %d with value %d\n",
           logicalAddress, physicalAddress, mainMemory[physicalAddress]);
}

void updateUI() {
    GtkTextBuffer *buffer;
    char text[1024];

    // Update page table text view
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tableTextView));
    snprintf(text, sizeof(text), "Page Number | Frame Number | Valid | Last Used\n----------------------------------------------\n");
    for (int i = 0; i < TOTAL_PAGES; i++) {
        snprintf(text + strlen(text), sizeof(text) - strlen(text), "%11d | %12d | %5d | %9d\n",
                 i, pageTable[i].frameNumber, pageTable[i].valid, pageTable[i].lastUsed);
    }
    gtk_text_buffer_set_text(buffer, text, -1);

    // Update main memory text view
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(memoryTextView));
    snprintf(text, sizeof(text), "Frame Number | Content\n----------------------\n");
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        snprintf(text + strlen(text), sizeof(text) - strlen(text), "%11d |", i);
        for (int j = 0; j < FRAME_SIZE; j++) {
            snprintf(text + strlen(text), sizeof(text) - strlen(text), " %2d", mainMemory[i * FRAME_SIZE + j]);
        }
        snprintf(text + strlen(text), sizeof(text) - strlen(text), "\n");
    }
    gtk_text_buffer_set_text(buffer, text, -1);

    // Update secondary memory text view
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(secondaryMemoryTextView));
    snprintf(text, sizeof(text), "Secondary Memory Content\n------------------------\n");
    for (int i = 0; i < SECONDARY_MEMORY_SIZE; i++) {
        if (i % FRAME_SIZE == 0) snprintf(text + strlen(text), sizeof(text) - strlen(text), "\nPage %d:\n", i / FRAME_SIZE);
        snprintf(text + strlen(text), sizeof(text) - strlen(text), "%3d", secondaryMemory[i]);
        if ((i + 1) % FRAME_SIZE == 0) snprintf(text + strlen(text), sizeof(text) - strlen(text), "\n");
    }
    gtk_text_buffer_set_text(buffer, text, -1);

    // Update page hits and faults labels
    snprintf(text, sizeof(text), "Page Hits: %d", pageHits);
    gtk_label_set_text(GTK_LABEL(hitLabel), text);
    snprintf(text, sizeof(text), "Page Faults: %d", pageFaults);
    gtk_label_set_text(GTK_LABEL(faultLabel), text);
}

void on_access_button_clicked(GtkWidget *widget, gpointer data) {
    const char *logicalAddressStr = gtk_entry_get_text(GTK_ENTRY(entry));
    int logicalAddress = atoi(logicalAddressStr);

    logicalAddressHistory[accessCount++] = logicalAddress;
    accessMemory(logicalAddress, pageTable, mainMemory, secondaryMemory, &nextFrame, pageReplacementAlgorithm, logicalAddressHistory, &accessCount);

    updateUI();
}

void on_algorithm_changed(GtkComboBox *comboBox, gpointer data) {
    pageReplacementAlgorithm = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(comboBox))[0];
}

int getReplacementIndex(char algorithm, PageTableEntry pageTable[], int nextFrame, int *logicalAddressHistory, int currentAccess) {
    if (algorithm == 'F' || algorithm == 'f') { // FIFO
        static int fifoPointer = 0;
        int replaceIndex = fifoQueue[fifoPointer];
        fifoPointer = (fifoPointer + 1) % TOTAL_FRAMES;
        return replaceIndex;
    } else if (algorithm == 'L' || algorithm == 'l') { // LRU
        int lruIndex = 0;
        int oldest = timeCounter;
        for (int i =0; i < TOTAL_PAGES; i++) {
            if (pageTable[i].valid && pageTable[i].lastUsed < oldest) {
                oldest = pageTable[i].lastUsed;
                lruIndex = i;
            }
        }
        return lruIndex;
    } else if (algorithm == 'O' || algorithm == 'o') { // Optimal
        int optIndex = 0;
        int farthest = -1;
        for (int i = 0; i < TOTAL_PAGES; i++) {
            if (pageTable[i].valid) {
                int j;
                for (j = currentAccess + 1; j < nextFrame; j++) {
                    if (logicalAddressHistory[j] / FRAME_SIZE == i) {
                        if (j > farthest) {
                            farthest = j;
                            optIndex = i;
                        }
                        break;
                    }
                }
                if (j == nextFrame) {
                    return i;
                }
            }
        }
        return optIndex;
    }
    return -1;
}

void on_reset_button_clicked(GtkWidget *widget, gpointer data) {
    initializePageTable(pageTable);
    memset(mainMemory, 0, sizeof(mainMemory));
    memset(fifoQueue, 0, sizeof(fifoQueue));
    fifoIndex = 0;
    timeCounter = 0;
    pageHits = 0;
    pageFaults = 0;
    nextFrame = 0;
    accessCount = 0;
    updateUI();
}
