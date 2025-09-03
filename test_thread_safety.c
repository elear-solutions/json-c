#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <json.h>

static void *
json_parser_thread(void *arg)
{
    const char *json_str = (const char *)arg;
    int thread_id = (int)(long)arg;
    
    printf("Thread %d: Starting JSON parsing\n", thread_id);
    
    for (int i = 0; i < 1000; i++) {
        struct json_object *j = json_tokener_parse(json_str);
        if (j == NULL) {
            printf("Thread %d: Failed to parse JSON at iteration %d\n", thread_id, i);
            abort();
        }
        if (json_object_get_type(j) != json_type_object) {
            printf("Thread %d: Expected object type at iteration %d\n", thread_id, i);
            abort();
        }
        json_object_put(j);
    }
    
    printf("Thread %d: Completed JSON parsing\n", thread_id);
    return NULL;
}

static void *
strtod_thread(void *arg)
{
    int thread_id = (int)(long)arg;
    
    printf("Thread %d: Starting strtod testing\n", thread_id);
    
    for (int i = 0; i < 1000; i++) {
        char *ptr;
        double result;
        errno = 0;
        result = strtod("1.5", &ptr);
        if (!(result == 1.5 && ptr == "1.5" + 3 && errno == 0)) {
            printf("Thread %d: strtod disturbed at iteration %d! result=%f, ptr=%s, errno=%d\n", 
                   thread_id, i, result, ptr, errno);
            abort();
        }
    }
    
    printf("Thread %d: Completed strtod testing\n", thread_id);
    return NULL;
}

static void *
sprintf_thread(void *arg)
{
    int thread_id = (int)(long)arg;
    
    printf("Thread %d: Starting sprintf testing\n", thread_id);
    
    for (int i = 0; i < 1000; i++) {
        char pointbuf[5];
        sprintf(pointbuf, "%#.0f", 1.0);
        if (!(pointbuf[1] == ',' || pointbuf[1] == '.')) {
            printf("Thread %d: sprintf disturbed at iteration %d! pointbuf='%s'\n", 
                   thread_id, i, pointbuf);
            abort();
        }
    }
    
    printf("Thread %d: Completed sprintf testing\n", thread_id);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (setlocale(LC_ALL, "fr_FR.UTF-8") == NULL) {
        printf("Failed to set locale to fr_FR.UTF-8\n");
        return 1;
    }
    
    printf("Testing JSON-C thread safety with locale: %s\n", setlocale(LC_ALL, NULL));
    
    const char *json1 = "{\"networkIds\": [\"test-concurrent-050\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
    const char *json2 = "{\"networkIds\": [\"test-concurrent-047\"], \"executablePath\": \"/bin/sleep\", \"argsOverrides\": [\"10\"]}";
    
    /* Create the threads */
    pthread_t json_thread1, json_thread2, strtod_thread1, sprintf_thread1;
    
    pthread_create(&json_thread1, NULL, json_parser_thread, (void *)json1);
    pthread_create(&json_thread2, NULL, json_parser_thread, (void *)json2);
    pthread_create(&strtod_thread1, NULL, strtod_thread, (void *)3);
    pthread_create(&sprintf_thread1, NULL, sprintf_thread, (void *)4);
    
    /* Let them run for a few seconds */
    struct timespec duration;
    duration.tv_sec = (argc > 1 ? atoi(argv[1]) : 3);
    duration.tv_nsec = 0;
    nanosleep(&duration, NULL);
    
    printf("Test completed successfully!\n");
    return 0;
}
