#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h> // For sleep()
#include <time.h>   // For srand()

// --- Configuration ---
#define NUM_STUDENTS 10       // Total number of students to simulate
#define MAX_CHAIRS 5          // Number of chairs in the waiting room [cite: 36]
#define TA_HELP_MIN_SECONDS 1 // Minimum time TA spends helping a student
#define TA_HELP_MAX_SECONDS 3 // Maximum time TA spends helping a student
#define STUDENT_ARRIVAL_MIN_SECONDS 0 // Min time before next student "arrives"
#define STUDENT_ARRIVAL_MAX_SECONDS 2 // Max time before next student "arrives"

// --- Semaphores and Mutex ---
sem_t waiting_room_chairs_sem;      // Limits students in waiting chairs [cite: 36]
sem_t student_present_for_ta_sem; // Student signals TA they are ready/present [cite: 38]
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
    printf("TA: دفتر باز است! آماده برای دانشجویان.\n");

    while (1) { // TA works indefinitely (or until all students are processed if we add such logic)
        printf("TA: در حال بررسی برای دانشجویان یا رفتن به حالت خواب...\n");
        sem_wait(&student_present_for_ta_sem); // Wait for a student to be present [cite: 40, 42]

        // A student is present and has taken a chair (and signaled).
        printf("TA: یک دانشجو حاضر است. او را فرا می‌خوانم.\n");
        sem_post(&ta_ready_for_student_sem); // Signal to the specific student that TA is ready [cite: 35]

        int help_duration = random_int(TA_HELP_MIN_SECONDS, TA_HELP_MAX_SECONDS);
        printf("TA: در حال کمک به یک دانشجو برای %d ثانیه...\n", help_duration);
        sleep(help_duration);

        printf("TA: کمک به دانشجو تمام شد.\n");
        sem_post(&consultation_finished_sem); // Signal that consultation for this student is over
                                             // TA will loop and wait for the next student [cite: 39, 40]
    }
    pthread_exit(NULL);
}

// --- Student Thread Function ---
void* student_thread_func(void* student_id_ptr) {
    int student_id = *(int*)student_id_ptr;
    free(student_id_ptr); // Free the allocated memory for the ID

    // Simulate random arrival time
    sleep(random_int(STUDENT_ARRIVAL_MIN_SECONDS, STUDENT_ARRIVAL_MAX_SECONDS));
    printf("دانشجوی %d: به دفتر TA رسید.\n", student_id);

    pthread_mutex_lock(&count_mutex);
    if (num_students_in_chairs < MAX_CHAIRS) { // Check if there's a chair available [cite: 36, 37]
        num_students_in_chairs++;
        sem_wait(&waiting_room_chairs_sem); // Take one of the available chair slots
        printf("دانشجوی %d: یک صندلی گرفت. (تعداد دانشجویان منتظر روی صندلی: %d)\n", student_id, num_students_in_chairs);
        pthread_mutex_unlock(&count_mutex);

        printf("دانشجوی %d: به TA اطلاع می‌دهد که آماده است.\n", student_id);
        sem_post(&student_present_for_ta_sem); // Announce presence to TA / Wake TA [cite: 38]

        sem_wait(&ta_ready_for_student_sem); // Wait for TA to be free and call this specific student [cite: 35]

        // Student is now with TA, so they leave their chair.
        sem_post(&waiting_room_chairs_sem); // Free up the chair slot

        pthread_mutex_lock(&count_mutex);
        num_students_in_chairs--;
        pthread_mutex_unlock(&count_mutex);

        printf("دانشجوی %d: در حال مشاوره با TA است.\n", student_id);
        sem_wait(&consultation_finished_sem); // Wait for TA to finish this consultation

        printf("دانشجوی %d: مشاوره تمام شد و دفتر را ترک می‌کند.\n", student_id);

    } else {
        // No chairs available [cite: 37]
        pthread_mutex_unlock(&count_mutex);
        printf("دانشجوی %d: صندلی خالی پیدا نکرد، دفتر را ترک می‌کند و بعداً مراجعه خواهد کرد.\n", student_id);
    }

    pthread_exit(NULL);
}

// --- Main Function ---
int main() {
    pthread_t ta_thread;
    pthread_t student_threads[NUM_STUDENTS];
    int i;

    srand(time(NULL)); // Seed random number generator

    // Initialize semaphores [cite: 43]
    sem_init(&waiting_room_chairs_sem, 0, MAX_CHAIRS); // 0: shared between threads, MAX_CHAIRS initial value
    sem_init(&student_present_for_ta_sem, 0, 0);
    sem_init(&ta_ready_for_student_sem, 0, 0);
    sem_init(&consultation_finished_sem, 0, 0);

    // Initialize mutex [cite: 43]
    pthread_mutex_init(&count_mutex, NULL);

    printf("شبیه‌سازی دفتر TA شروع شد. کل صندلی‌های انتظار: %d\n", MAX_CHAIRS);
    printf("تعداد کل دانشجویان: %d\n\n", NUM_STUDENTS);

    // Create TA thread [cite: 42]
    if (pthread_create(&ta_thread, NULL, ta_thread_func, NULL) != 0) {
        perror("Failed to create TA thread");
        return 1;
    }

    // Create student threads [cite: 41]
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
        if (student_threads[i]) { // Check if thread was created successfully (simple check)
            pthread_join(student_threads[i], NULL);
        }
    }

    printf("\nهمه دانشجویان پردازش شده‌اند یا دفتر را ترک کرده‌اند.\n");
    printf("TA به کار خود ادامه می‌دهد (برای خاتمه برنامه از Ctrl+C استفاده کنید یا منطق خاتمه TA را پیاده‌سازی کنید).\n");

    // In a real scenario, you might want a way to signal the TA thread to terminate.
    // For this simulation, we let it run and terminate the program manually.
    // pthread_cancel(ta_thread); // Or a more graceful shutdown mechanism
    // pthread_join(ta_thread, NULL);


    // Destroy semaphores and mutex
    sem_destroy(&waiting_room_chairs_sem);
    sem_destroy(&student_present_for_ta_sem);
    sem_destroy(&ta_ready_for_student_sem);
    sem_destroy(&consultation_finished_sem);
    pthread_mutex_destroy(&count_mutex);

    return 0;
}