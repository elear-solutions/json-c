#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <json.h>

#define NUM_THREADS 4
#define ITERATIONS 1000000

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int found_issue = 0;

static void *
json_parser_thread_simple(void *arg)
{
    int thread_id = (int)(long)arg;
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting simple JSON parsing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    // Print the JSON strings we'll be using for verification
    const char *json_str1 = "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
    const char *json_str2 = "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
    
    if (thread_id == 0) {
        pthread_mutex_lock(&print_mutex);
        printf("DEBUG: JSON string 1: %s\n", json_str1);
        printf("DEBUG: JSON string 2: %s\n", json_str2);
        pthread_mutex_unlock(&print_mutex);
    }
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < ITERATIONS && !found_issue; i++) {
        // Use the exact JSON strings from your application
        const char *json_str = (i % 2 == 0) ? json_str1 : json_str2;
        
        // Create a new token each time to avoid complex state issues
        struct json_tokener *tok = json_tokener_new();
        if (!tok) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: Failed to create token at iteration %d\n", thread_id, i);
            pthread_mutex_unlock(&print_mutex);
            continue;
        }
        
        struct json_object *j = json_tokener_parse_ex(tok, json_str, -1);
        
        if (j == NULL) {
            enum json_tokener_error err = json_tokener_get_error(tok);
            error_count++;
            
            if (err == json_tokener_error_parse_comment) {
                pthread_mutex_lock(&print_mutex);
                printf("Thread %d: SUCCESS! Found 'expected comment' error at iteration %d\n", thread_id, i);
                printf("Thread %d: This reproduces the thread safety issue!\n", thread_id);
                found_issue = 1;
                pthread_mutex_unlock(&print_mutex);
                json_tokener_free(tok);
                return (void*)(long)1; // Return success code
            }
            
            if (error_count % 10000 == 0) {
                pthread_mutex_lock(&print_mutex);
                printf("Thread %d: Error count: %d, last error: %s\n", 
                       thread_id, error_count, json_tokener_error_desc(err));
                pthread_mutex_unlock(&print_mutex);
            }
        } else {
            if (json_object_get_type(j) != json_type_object) {
                pthread_mutex_lock(&print_mutex);
                printf("Thread %d: Expected object type at iteration %d\n", thread_id, i);
                pthread_mutex_unlock(&print_mutex);
            } else {
                success_count++;
            }
            json_object_put(j);
        }
        
        json_tokener_free(tok);
        
        // Add small random delays to increase contention
        if (i % 1000 == 0) {
            struct timespec delay = {0, (thread_id % 5) * 1000}; // Small variable delay
            nanosleep(&delay, NULL);
        }
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed - Success: %d, Errors: %d\n", thread_id, success_count, error_count);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

static void *
strtod_thread_simple(void *arg)
{
    int thread_id = (int)(long)arg;
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting strtod testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    for (int i = 0; i < ITERATIONS && !found_issue; i++) {
        char *ptr;
        double result;
        errno = 0;
        result = strtod("1.5", &ptr);
        if (!(result == 1.5 && ptr == "1.5" + 3 && errno == 0)) {
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: strtod disturbed at iteration %d! result=%f, ptr=%s, errno=%d\n", 
                   thread_id, i, result, ptr, errno);
            pthread_mutex_unlock(&print_mutex);
            found_issue = 1;
            return (void*)(long)1;
        }
        
        // Add small delays
        if (i % 1000 == 0) {
            struct timespec delay = {0, 1000};
            nanosleep(&delay, NULL);
        }
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed strtod testing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

int main(int argc, char *argv[])
{
    // Set a non-C numeric locale to trigger the thread safety issue
    // This simulates a system where the numeric locale is not "C"
    if (setlocale(LC_NUMERIC, "fr_FR.UTF-8") == NULL) {
        printf("Failed to set LC_NUMERIC to fr_FR.UTF-8, trying fr_FR\n");
        if (setlocale(LC_NUMERIC, "fr_FR") == NULL) {
            printf("Failed to set LC_NUMERIC to fr_FR, trying de_DE\n");
            if (setlocale(LC_NUMERIC, "de_DE") == NULL) {
                printf("Failed to set LC_NUMERIC, using current locale\n");
            }
        }
    }
    
    printf("Testing JSON-C thread safety with system locale: %s\n", setlocale(LC_ALL, NULL));
    
    // Check what the numeric locale is currently set to
    char *numeric_locale = setlocale(LC_NUMERIC, NULL);
    printf("Current LC_NUMERIC locale: %s\n", numeric_locale ? numeric_locale : "NULL");
    
    // If the numeric locale is not "C", then json-c will call setlocale(LC_NUMERIC, "C")
    // which can cause the thread safety issue
    if (numeric_locale && strcmp(numeric_locale, "C") != 0) {
        printf("System has non-C numeric locale - this will trigger the thread safety issue!\n");
    } else {
        printf("System has C numeric locale - may need to set a different locale to reproduce the issue\n");
    }
    printf("Using %d threads with %d iterations each\n", NUM_THREADS, ITERATIONS);
    printf("This is a simplified test to reproduce the thread safety issue...\n");
    
    pthread_t threads[NUM_THREADS];
    
    // Create threads - 2 JSON parsers, 2 strtod testers
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i < 2) {
            // First 2 threads do JSON parsing
            pthread_create(&threads[i], NULL, json_parser_thread_simple, (void *)(long)i);
        } else {
            // Next 2 threads do strtod testing
            pthread_create(&threads[i], NULL, strtod_thread_simple, (void *)(long)i);
        }
    }
    
    // Wait for all threads to complete
    int issue_found = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void *result;
        pthread_join(threads[i], &result);
        if (result == (void*)(long)1) {
            issue_found = 1;
        }
    }
    
    if (issue_found) {
        printf("SUCCESS: Thread safety issue was reproduced!\n");
        printf("This confirms the need for the fix.\n");
    } else {
        printf("Test completed without reproducing the issue.\n");
        printf("This is normal - race conditions are hard to reproduce consistently.\n");
        printf("The fix is still needed based on your real application evidence.\n");
    }
    
    return 0;
}
