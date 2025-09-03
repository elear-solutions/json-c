#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json.h>

#define NUM_THREADS 8
#define ITERATIONS 10000

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *
json_parser_thread_reuse(void *arg)
{
    int thread_id = (int)(long)arg;
    struct json_tokener *tok = json_tokener_new();
    
    if (!tok) {
        pthread_mutex_lock(&print_mutex);
        printf("Thread %d: Failed to create token\n", thread_id);
        pthread_mutex_unlock(&print_mutex);
        return NULL;
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting JSON parsing with token reuse\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    for (int i = 0; i < ITERATIONS; i++) {
        const char *json_str = (i % 2 == 0) ? 
            "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}" :
            "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
        
        json_tokener_reset(tok);
        struct json_object *j = json_tokener_parse_ex(tok, json_str, -1);
        
        if (j == NULL) {
            enum json_tokener_error err = json_tokener_get_error(tok);
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: Failed to parse JSON at iteration %d, error: %s\n", 
                   thread_id, i, json_tokener_error_desc(err));
            pthread_mutex_unlock(&print_mutex);
            
            if (err == json_tokener_error_parse_comment) {
                printf("Thread %d: Found 'expected comment' error - this might be the thread safety issue!\n", thread_id);
            }
            continue;
        }
        
        if (json_object_get_type(j) != json_type_object) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: Expected object type at iteration %d\n", thread_id, i);
            pthread_mutex_unlock(&print_mutex);
            json_object_put(j);
            continue;
        }
        
        json_object_put(j);
        
        // Add some small delay to increase contention
        struct timespec delay = {0, 1000}; // 1 microsecond
        nanosleep(&delay, NULL);
    }
    
    json_tokener_free(tok);
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed JSON parsing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

static void *
strtod_thread_aggressive(void *arg)
{
    int thread_id = (int)(long)arg;
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting aggressive strtod testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    for (int i = 0; i < ITERATIONS; i++) {
        char *ptr;
        double result;
        errno = 0;
        result = strtod("1.5", &ptr);
        if (!(result == 1.5 && ptr == "1.5" + 3 && errno == 0)) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: strtod disturbed at iteration %d! result=%f, ptr=%s, errno=%d\n", 
                   thread_id, i, result, ptr, errno);
            pthread_mutex_unlock(&print_mutex);
            abort();
        }
        
        // Add some small delay
        struct timespec delay = {0, 1000};
        nanosleep(&delay, NULL);
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed strtod testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

static void *
sprintf_thread_aggressive(void *arg)
{
    int thread_id = (int)(long)arg;
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting aggressive sprintf testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    for (int i = 0; i < ITERATIONS; i++) {
        char pointbuf[5];
        sprintf(pointbuf, "%#.0f", 1.0);
        if (!(pointbuf[1] == ',' || pointbuf[1] == '.')) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: sprintf disturbed at iteration %d! pointbuf='%s'\n", 
                   thread_id, i, pointbuf);
            pthread_mutex_unlock(&print_mutex);
            abort();
        }
        
        // Add some small delay
        struct timespec delay = {0, 1000};
        nanosleep(&delay, NULL);
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed sprintf testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (setlocale(LC_ALL, "fr_FR.UTF-8") == NULL) {
        printf("Failed to set locale to fr_FR.UTF-8\n");
        return 1;
    }
    
    printf("Testing JSON-C thread safety with locale: %s\n", setlocale(LC_ALL, NULL));
    printf("Using %d threads with %d iterations each\n", NUM_THREADS, ITERATIONS);
    
    pthread_t threads[NUM_THREADS];
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i < 4) {
            // First 4 threads do JSON parsing with token reuse
            pthread_create(&threads[i], NULL, json_parser_thread_reuse, (void *)(long)i);
        } else if (i < 6) {
            // Next 2 threads do strtod testing
            pthread_create(&threads[i], NULL, strtod_thread_aggressive, (void *)(long)i);
        } else {
            // Last 2 threads do sprintf testing
            pthread_create(&threads[i], NULL, sprintf_thread_aggressive, (void *)(long)i);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Test completed successfully!\n");
    return 0;
}
