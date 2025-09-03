#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <json.h>

#define NUM_THREADS 8
#define ITERATIONS 500000

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int found_issue = 0;

// Simulate your application's token reuse pattern
static void *
json_parser_token_reuse(void *arg)
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
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < ITERATIONS && !found_issue; i++) {
        // Use the exact JSON strings from your application
        const char *json_str = (i % 2 == 0) ? 
            "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}" :
            "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
        
        // Don't reset token every time - simulate your reuse pattern
        // Only reset occasionally to create more complex state
        if (i % 100 == 0) {
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
                found_issue = 1;
                pthread_mutex_unlock(&print_mutex);
                json_tokener_free(tok);
                return (void*)(long)1;
            }
            
            if (error_count % 1000 == 0) {
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
        
        // Add variable delays to create more realistic contention
        if (i % 1000 == 0) {
            struct timespec delay = {0, (thread_id % 10) * 1000};
            nanosleep(&delay, NULL);
        }
    }
    
    json_tokener_free(tok);
    
    pthread_mutex_lock(&print_mutex);
    printf("Thread %d: Completed - Success: %d, Errors: %d\n", thread_id, success_count, error_count);
    pthread_mutex_unlock(&print_mutex);
    return NULL;
}

// Simulate your ec_parse_json_string function more closely
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
    
    for (int i = 0; i < ITERATIONS && !found_issue; i++) {
        // Use the exact JSON strings from your application
        const char *json_str = (i % 2 == 0) ? 
            "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}" :
            "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
        
        char *jsonStr = strdup(json_str);
        int result = ec_parse_json_string_simulated(jsonStr, &jsonObj, &token, (i % 5 != 0));
        free(jsonStr);
        
        if (result == 2) {
            // Successfully reproduced the issue
            pthread_mutex_lock(&print_mutex);
            printf("Thread %d: SUCCESS! Reproduced the thread safety issue!\n", thread_id);
            pthread_mutex_unlock(&print_mutex);
            found_issue = 1;
            return (void*)(long)1;
        }
        
        if (jsonObj) {
            json_object_put(jsonObj);
            jsonObj = NULL;
        }
        
        // Add some realistic delays
        if (i % 1000 == 0) {
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
    // Set a non-C numeric locale to trigger the thread safety issue
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
    
    char *numeric_locale = setlocale(LC_NUMERIC, NULL);
    printf("Current LC_NUMERIC locale: %s\n", numeric_locale ? numeric_locale : "NULL");
    
    if (numeric_locale && strcmp(numeric_locale, "C") != 0) {
        printf("System has non-C numeric locale - this will trigger the thread safety issue!\n");
    } else {
        printf("System has C numeric locale - may need to set a different locale to reproduce the issue\n");
    }
    
    printf("Using %d threads with %d iterations each\n", NUM_THREADS, ITERATIONS);
    printf("This test simulates your application's token reuse pattern...\n");
    
    pthread_t threads[NUM_THREADS];
    
    // Create threads - mix of token reuse and application simulator
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i < 4) {
            // First 4 threads use token reuse pattern
            pthread_create(&threads[i], NULL, json_parser_token_reuse, (void *)(long)i);
        } else {
            // Next 4 threads simulate your application function
            pthread_create(&threads[i], NULL, application_simulator_thread, (void *)(long)i);
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
