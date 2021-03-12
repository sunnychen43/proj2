#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "../rpthread.h"

void *a(void *ptr) {
	int *num = (int *)ptr;
	*num = 10;
	return NULL;
}

int main() {
	int *b = malloc(sizeof(int));
	int *c = malloc(sizeof(int));
	printf("a\n");
	pthread_t thread;
	pthread_create(&thread, NULL, &a, b);

	pthread_join(thread, &c);
	printf("asdf\n");
	printf("%d %d\n", *b, c);

	return 0;
}

// void reg_calc() {
// 	int n = 5000;
// 	int *arr = malloc(n * sizeof(*arr));

// 	for (int i=0; i < n; i++) {
// 		for (int j=0; j < n; j++) {
// 			arr[i] = i * j;
// 		}
// 	}
// 	printf("reg done\n");
// }

// void bad_calc() {
// 	int n = 10000;
// 	int *arr = malloc(n * sizeof(*arr));

// 	for (int i=0; i < n; i++) {
// 		for (int j=0; j < n; j++) {
// 			arr[i] = i * j;
// 			if (j % 1000 == 0)
// 				rpthread_yield();
// 		}
// 	}
// 	printf("bad done\n");
// }

// int main(int argc, char **argv) {
	
// 	int thread_num = 10;
// 	pthread_t thread[10];

// 	struct timespec start, end;
//     clock_gettime(CLOCK_REALTIME, &start);

// 	for (int i = 0; i < 1; ++i)
// 		pthread_create(&thread[i], NULL, &bad_calc, NULL);

// 	for (int i = 1; i < 10; ++i)
// 		pthread_create(&thread[i], NULL, &reg_calc, NULL);

// 	for (int i = 0; i < thread_num; ++i)
// 		pthread_join(thread[i], NULL);

// 	clock_gettime(CLOCK_REALTIME, &end);
//         printf("running time: %lu micro-seconds\n", 
// 	       (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000);
// 	return 0;
// }
