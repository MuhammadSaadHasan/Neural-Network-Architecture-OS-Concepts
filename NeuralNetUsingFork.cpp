#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

using namespace std;

struct ThreadData 
{
    int thread_id;
    int process_num;
    int pipefd[2];
    pthread_mutex_t pipe_mutex;
};

const int NumOfProcesses = 3;				//test values
const int NumOfThreads = 2;
ThreadData T1[NumOfProcesses][NumOfThreads];

int num[2] ={30,40};
int w[4] = {2,3,4,5};					//test values

void* Function(void* arg) {
    int* arr = reinterpret_cast<int*>(arg);
    int thread_id = arr[0];
    int process_num = arr[1];
    cout << "Process " << process_num << ", Thread " << thread_id << endl;
    if (process_num == 0)								//make these generic with formula
    {
            pthread_mutex_lock(&T1[process_num][thread_id].pipe_mutex); 
        for(int i = 0; i < NumOfThreads;i++)
        {
                close(T1[process_num][thread_id].pipefd[0]); 
                int number = num[i]*w[i];
                cout << "NUm being sent == " << number << endl;
                write(T1[process_num][thread_id].pipefd[1], &number, sizeof(number)); 
                close(T1[process_num][thread_id].pipefd[1]);
        }
        pthread_mutex_unlock(&T1[process_num][thread_id].pipe_mutex);
    }

    if (process_num == 1)
    {
        int i = thread_id;
    	pthread_mutex_lock(&T1[process_num][thread_id].pipe_mutex); 
        int result = 0;
		for (int i = 0; i < NumOfThreads; i++)
		{
		        int received_num;
		        close(T1[process_num - 1][i].pipefd[1]);
		        read(T1[process_num - 1][i].pipefd[0], &received_num, sizeof(received_num));
		        result += received_num;
		        close(T1[process_num - 1][i].pipefd[0]);
		}
    	pthread_mutex_unlock(&T1[process_num][thread_id].pipe_mutex); 
        cout << "Result is ==  = = " << result << endl;
    }

    sleep(1); 
    pthread_exit(nullptr);
}

int main() 
{
    cout << "Parent" << endl;
    for (int i = 0; i < NumOfProcesses; ++i){
        for (int j = 0; j < NumOfThreads; ++j){
            pipe(T1[i][j].pipefd); 
            T1[i][j].thread_id = j;
            T1[i][j].process_num = i;
            pthread_mutex_init(&T1[i][j].pipe_mutex, nullptr);
        }
    }
    for (int i = 0; i < NumOfProcesses; i++) {
	    sleep(NumOfProcesses    + 1);
	    pid_t pid = fork();
	    if (pid == 0) {
		cout << "\nIn process number = " << i << endl;
		pthread_t threads[NumOfThreads];
		for (int j = 0; j < NumOfThreads; j++) {
		    int arr[2];
		    arr[0] = j;
		    arr[1] = i;

		    pthread_create(&threads[j], nullptr, Function, reinterpret_cast<void*>(arr));
		    sleep(1);
		}
		while (true)
		    sleep(1);
	    }
	}

	sleep(NumOfProcesses + 1);
	return 0;

}
