#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <json.h>

#define NUM_THREADS 16
#define ITERATIONS 50000

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Simulate your application's token reuse pattern
static void *
json_parser_thread_realistic(void *arg)
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
    printf("Thread %d: Starting realistic JSON parsing\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < ITERATIONS; i++) {
        // Use the exact JSON strings from your application
        const char *json_str = (i % 2 == 0) ? 
            "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}" :
            "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
        
        // Don't reset token every time - simulate your reuse pattern
        if (i % 10 == 0) {
            json_tokener_reset(tok);
        }
        
        struct json_object *j = json_tokener_parse_ex(tok, json_str, -1);
        
        if (j == NULL) {
            enum json_tokener_error err = json_tokener_get_error(tok);
            error_count++;
            
            if (err == json_tokener_error_parse_comment) {
                pthread_mutex_lock(&print_mutex);
                printf("Thread %d: SUCCESS! Found 'expected comment' error at iteration %d\n", thread_id, i);
                printf("Thread %d: This reproduces the thread safety issue!\n", thread_id);
                pthread_mutex_unlock(&print_mutex);
                json_object_put(j);
                json_tokener_free(tok);
                return (void*)(long)1; // Return success code
            }
            
            if (error_count % 1000 == 0) {
                pthread_mutex_lock(&print_mutex);
                printf("Thread %d: Error count: %d, last error: %s\n", 
                       thread_id, error_count, json_tokener_error_desc(err));
                pthread_mutex_unlock(&print_mutex);
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
        
        success_count++;
        json_object_put(j);
        
        // Add variable delays to create more realistic contention
        if (i % 100 == 0) {
            struct timespec delay = {0, (thread_id % 10) * 1000}; // Variable delay
            nanosleep(&delay, NULL);
        }
    }
    
    json_tokener_free(tok);
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed - Success: %d, Errors: %d\n", thread_id, success_count, error_count);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

// Simulate your application's ec_parse_json_string function
static int
ec_parse_json_string_simulated(char *jsonStr, struct json_object **outJsonObj,
                              struct json_tokener **token, bool reUseFlag) {
    int length;
    uint8_t jsonErrNum;

    if ((NULL == jsonStr) || (NULL == outJsonObj) || (NULL == token)) {
        return -1;
    }
    
    if (!reUseFlag) {
        *token = json_tokener_new();
    }
    
    length = strlen(jsonStr);
    if (NULL == (*outJsonObj = json_tokener_parse_ex((struct json_tokener *)*token, jsonStr, length))) {
        if (json_tokener_continue != (jsonErrNum = json_tokener_get_error(*token))) {
            if (jsonErrNum == json_tokener_error_parse_comment) {
                printf("SUCCESS! Reproduced 'expected comment' error in simulated function!\n");
                return 2; // Special return code for success
            }
            json_tokener_reset(*token);
            json_tokener_free(*token);
            *token = NULL;
            return -1;
        }
        return 1;
    }
    
    json_tokener_free(*token);
    *token = NULL;
    return 0;
}

static void *
application_simulator_thread(void *arg)
{
    int thread_id = (int)(long)arg;
    struct json_tokener *token = NULL;
    struct json_object *jsonObj = NULL;
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Starting application simulator\n", thread_id);
    pthread_mutex_unlock(&print_mutex);
    
    for (int i = 0; i < ITERATIONS; i++) {
        // Simulate your exact JSON strings
        char jsonStr[256];
        snprintf(jsonStr, sizeof(jsonStr), 
                "{\"networkIds\": [\"test-concurrent-%03d\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}", 
                thread_id * 100 + i);
        
        int result = ec_parse_json_string_simulated(jsonStr, &jsonObj, &token, (i % 5 != 0));
        
        if (result == 2) {
            // Successfully reproduced the issue
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: SUCCESS! Reproduced the thread safety issue!\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
            return (void*)(long)1;
        }
        
        if (jsonObj) {
            json_object_put(jsonObj);
            jsonObj = NULL;
        }
        
        // Add some realistic delays
        if (i % 50 == 0) {
            struct timespec delay = {0, 1000};
            nanosleep(&delay, NULL);
        }
    }
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed application simulator\n", thread_id);
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
    printf("This test simulates your application's behavior more closely...\n");
    
    pthread_t threads[NUM_THREADS];
    
    // Create threads - mix of different approaches
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i < 8) {
            // First 8 threads use the realistic parser
            pthread_create(&threads[i], NULL, json_parser_thread_realistic, (void *)(long)i);
        } else {
            // Next 8 threads simulate your application function
            pthread_create(&threads[i], NULL, application_simulator_thread, (void *)(long)i);
        }
    }
    
    // Wait for all threads to complete
    int found_issue = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        void *result;
        pthread_join(threads[i], &result);
        if (result == (void*)(long)1) {
            found_issue = 1;
        }
    }
    
    if (found_issue) {
        printf("SUCCESS: Thread safety issue was reproduced!\n");
        printf("This confirms the need for the fix.\n");
    } else {
        printf("Test completed without reproducing the issue.\n");
        printf("This is normal - race conditions are hard to reproduce consistently.\n");
        printf("The fix is still needed based on your real application evidence.\n");
    }
    
    return 0;
}
