#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h> // For sleep()
#include <time.h>   // For srand()

// --- Configuration ---
#define NUM_STUDENTS 10       // Total number of students to simulate
#define MAX_CHAIRS 5          // Number of chairs in the waiting room 
#define TA_HELP_MIN_SECONDS 1 // Minimum time TA spends helping a student
#define TA_HELP_MAX_SECONDS 3 // Maximum time TA spends helping a student
#define STUDENT_ARRIVAL_MIN_SECONDS 0 // Min time before next student "arrives"
#define STUDENT_ARRIVAL_MAX_SECONDS 2 // Max time before next student "arrives"

// --- Semaphores and Mutex ---
sem_t waiting_room_chairs_sem;      // Limits students in waiting chairs 
sem_t student_present_for_ta_sem; // Student signals TA they are ready/present 
sem_t ta_ready_for_student_sem;     // TA signals they are ready for the specific student
sem_t consultation_finished_sem;  // TA signals consultation with current student is over

pthread_mutex_t count_mutex;        // Mutex to protect num_students_in_chairs
int num_students_in_chairs = 0;     // Counter for students currently in chairs

// --- Utility Function ---
// Generates a random number between min and max (inclusive)
int random_int(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    return (rand() % (max - min + 1)) + min;
}

// --- TA Thread Function ---
void* ta_thread_func(void* arg) {
    printf("TA: Office is open! Ready for students.\n");

    while (1) { // TA works indefinitely (or until all students are processed if we add such logic)
        printf("TA: Checking for students or going to sleep...\n");
        sem_wait(&student_present_for_ta_sem); // Wait for a student to be present 

        // A student is present and has taken a chair (and signaled).
        printf("TA: A student is present. Calling them in.\n");
        sem_post(&ta_ready_for_student_sem); // Signal to the specific student that TA is ready 

        int help_duration = random_int(TA_HELP_MIN_SECONDS, TA_HELP_MAX_SECONDS);
        printf("TA: Helping a student for %d seconds...\n", help_duration);
        sleep(help_duration);

        printf("TA: Finished helping the student.\n");
        sem_post(&consultation_finished_sem); // Signal that consultation for this student is over
                                              // TA will loop and wait for the next student 
    }
    pthread_exit(NULL);
}

// --- Student Thread Function ---
void* student_thread_func(void* student_id_ptr) {
    int student_id = *(int*)student_id_ptr;
    free(student_id_ptr); // Free the allocated memory for the ID

    // Simulate random arrival time
    sleep(random_int(STUDENT_ARRIVAL_MIN_SECONDS, STUDENT_ARRIVAL_MAX_SECONDS));
    printf("Student %d: Arrived at TA's office.\n", student_id);

    pthread_mutex_lock(&count_mutex);
    if (num_students_in_chairs < MAX_CHAIRS) { // Check if there's a chair available
        num_students_in_chairs++;
        sem_wait(&waiting_room_chairs_sem); // Take one of the available chair slots
        printf("Student %d: Took a chair. (Waiting students in chairs: %d)\n", student_id, num_students_in_chairs);
        pthread_mutex_unlock(&count_mutex);

        printf("Student %d: Informing TA they are ready.\n", student_id);
        sem_post(&student_present_for_ta_sem); // Announce presence to TA / Wake TA 

        sem_wait(&ta_ready_for_student_sem); // Wait for TA to be free and call this specific student 

        // Student is now with TA, so they leave their chair.
        sem_post(&waiting_room_chairs_sem); // Free up the chair slot

        pthread_mutex_lock(&count_mutex);
        num_students_in_chairs--;
        pthread_mutex_unlock(&count_mutex);

        printf("Student %d: Consulting with TA.\n", student_id);
        sem_wait(&consultation_finished_sem); // Wait for TA to finish this consultation

        printf("Student %d: Consultation finished. Leaving the office.\n", student_id);

    } else {
        // No chairs available 
        pthread_mutex_unlock(&count_mutex);
        printf("Student %d: No chairs available. Leaving and will come back later.\n", student_id);
    }

    pthread_exit(NULL);
}

// --- Main Function ---
int main() {
    pthread_t ta_thread;
    pthread_t student_threads[NUM_STUDENTS];
    int i;

    srand(time(NULL)); // Seed random number generator

    // Initialize semaphores
    sem_init(&waiting_room_chairs_sem, 0, MAX_CHAIRS); // 0: shared between threads, MAX_CHAIRS initial value
    sem_init(&student_present_for_ta_sem, 0, 0);
    sem_init(&ta_ready_for_student_sem, 0, 0);
    sem_init(&consultation_finished_sem, 0, 0);

    // Initialize mutex 
    pthread_mutex_init(&count_mutex, NULL);

    printf("TA Office Simulation Started. Total waiting chairs: %d\n", MAX_CHAIRS);
    printf("Total number of students: %d\n\n", NUM_STUDENTS);

    // Create TA thread 
    if (pthread_create(&ta_thread, NULL, ta_thread_func, NULL) != 0) {
        perror("Failed to create TA thread");
        return 1;
    }

    // Create student threads 
    for (i = 0; i < NUM_STUDENTS; i++) {
        int* student_id = malloc(sizeof(int));
        if (student_id == NULL) {
            perror("Failed to allocate memory for student ID");
            continue; // Skip this student if allocation fails
        }
        *student_id = i + 1; // Student IDs from 1 to N

        if (pthread_create(&student_threads[i], NULL, student_thread_func, student_id) != 0) {
            perror("Failed to create student thread");
            free(student_id); // Free memory if thread creation fails
        }
        // Small delay between student thread creations to slightly stagger arrivals further
        // This is optional as random sleep is already in student_thread_func
        // sleep(random_int(0,1));
    }

    // Wait for all student threads to complete
    for (i = 0; i < NUM_STUDENTS; i++) {
        // A more robust check would be to see if pthread_create succeeded for student_threads[i]
        // For simplicity, assuming all intended threads were stored if no error printed.
         if (student_threads[i] != 0) { // Basic check if thread identifier is not null
            pthread_join(student_threads[i], NULL);
        }
    }

    printf("\nAll students have been processed or have left the office.\n");
    printf("TA will continue running (Press Ctrl+C to terminate or implement TA termination logic).\n");

    // In a real scenario, you might want a way to signal the TA thread to terminate.
    // For this simulation, we let it run and terminate the program manually or let OS clean up.
    // pthread_cancel(ta_thread); // Forcibly cancel TA thread (use with caution)
    // pthread_join(ta_thread, NULL); // Wait for TA thread to finish if it has an exit condition


    // Destroy semaphores and mutex
    sem_destroy(&waiting_room_chairs_sem);
    sem_destroy(&student_present_for_ta_sem);
    sem_destroy(&ta_ready_for_student_sem);
    sem_destroy(&consultation_finished_sem);
    pthread_mutex_destroy(&count_mutex);

    return 0;
}
