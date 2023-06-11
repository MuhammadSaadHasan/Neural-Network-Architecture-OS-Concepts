#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <sstream>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define BIAS 0.0
#define LayerLength 7
#define STACK_SIZE 1024 * 1024

using namespace std;

const char *pipe_name = "Back Track Pipe";
sem_t semaphore;
bool executionFlag = true;
int layerPosition = 1;
int pipeFD[2];

struct DataToPass
{
    vector<vector<vector<float>>> X;
    float A;
    int fd0;
    int fd1;
    int index;
    int position;

    DataToPass(vector<vector<vector<float>>> X, float A, int i, int FD0, int FD1, int position)
    {
        this->X = X;
        this->A = A;
        this->index = i;
        this->fd0 = FD0;
        this->fd1 = FD1;
        this->position = position;
    }
};
// ----------------- Transpose Function -----------------
vector<vector<float>> transposeMatrix(const vector<vector<float>> &matrix)
{
    int n = matrix.size();
    int m = matrix[0].size();
    vector<vector<float>> result(m, vector<float>(n));
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < m; ++j)
        {
            result[j][i] = matrix[i][j];
        }
    }
    return result;
}

// ----------------- Neural Network Data -----------------
class NeuralNetworkData
{
public:
    vector<float> neurons;
    vector<vector<vector<float>>> weights;

    NeuralNetworkData(const string &neuronFile, const string &weightFile)
    {
        readNeuronData(neuronFile);
        readWeightData(weightFile);
    }

    void printWeights()
    {
        for (int i = 0; i < weights.size(); ++i)
        {
            cout << "---------------Layer " << i << " " << endl;
            for (int j = 0; j < weights[i].size(); ++j)
            {
                for (int k = 0; k < weights[i][j].size(); ++k)
                {
                    cout << weights[i][j][k] << " ";
                }
                cout << endl;
            }
        }
    }

    void printNeurons()
    {
        for (int i = 0; i < neurons.size(); ++i)
        {
            cout << neurons[i] << " ";
        }
        cout << endl;
    }

private:
    void readNeuronData(const string &filename)
    {
        ifstream file(filename);
        string line;
        while (getline(file, line))
        {
            if (line.empty())
            {
                break;
            }
            neurons.push_back(stof(line));
        }
    }

    void readWeightData(const string &filename)
    {
        int fileIndex = stoi(filename.substr(filename.size() - 5));
        string baseFileName = filename.substr(0, filename.size() - 5);

        for (size_t i = 0; i < LayerLength; ++i)
        {
            vector<vector<float>> matrix;
            string currentFile = baseFileName + to_string(fileIndex) + ".txt";
            ifstream file(currentFile);
            string line;
            while (getline(file, line))
            {
                if (line.empty())
                {
                    break;
                }
                vector<float> row;
                stringstream ss(line);
                string value;
                while (getline(ss, value, ','))
                {
                    row.push_back(stof(value));
                }
                matrix.push_back(row);
            }
            weights.push_back(transposeMatrix(matrix));
            fileIndex++;
        }
    }
};

// ----------------- Thread Calculation Structure -----------------
struct ThreadArgs
{
    vector<vector<vector<float>>> weights;
    float neuronValue;
    int neuronIndex;
    int readFd;
    int writeFd;
    int layerIndex;

    ThreadArgs(const vector<vector<vector<float>>> &weights, float neuronValue, int neuronIndex, int readFd, int writeFd, int layerIndex)
        : weights(weights), neuronValue(neuronValue), neuronIndex(neuronIndex), readFd(readFd), writeFd(writeFd), layerIndex(layerIndex) {}
};

// ----------------- Thread Calculation Function -----------------
void *calculate(void *arg)
{
    sem_wait(&semaphore);
    auto *threadArgs = static_cast<ThreadArgs *>(arg);

    vector<float> values;
    size_t vectorSize;
    int readFd = threadArgs->readFd;
    int writeFd = threadArgs->writeFd;

    read(readFd, &vectorSize, sizeof(vectorSize));
    values.resize(vectorSize); // Read the vector elements from the pipe
    read(readFd, values.data(), vectorSize * sizeof(float));

    // Perform the calculations
    for (int i = 0; i < threadArgs->weights[threadArgs->layerIndex].size(); ++i)
    {
        values[i] += threadArgs->neuronValue * threadArgs->weights[threadArgs->layerIndex][i][threadArgs->neuronIndex] + BIAS;
    }

    vectorSize = values.size();

    // Write the vector size to the pipe
    write(writeFd, &vectorSize, sizeof(vectorSize));
    // Write the vector elements to the pipe
    write(writeFd, values.data(), vectorSize * sizeof(float));

    sem_post(&semaphore);

    return nullptr;
}

int childFunction(void *arg);

// ----------------- Main Function -----------------
int main(int argc, char *argv[])
{

    // Create a named pipe
    mkfifo(pipe_name, 0666);
    mkfifo("pipe2", 0666);
    sem_init(&semaphore, 0, 1);
    pipe(pipeFD);
    bool isLastLayer = (layerPosition == LayerLength);
    NeuralNetworkData neuralNetworkData("Neurons1.txt", "Weights1.txt");

    if (argc > 1)
    {
        executionFlag = (stoi(argv[1]) == 1);
    }
    if (argc > 2)
    {
        layerPosition = stoi(argv[2]);
    }
    isLastLayer = (layerPosition == LayerLength);
    // cout << "isLastLayer " << isLastLayer << endl;
    if (isLastLayer)
    {
        string fdString = argv[3];
        size_t separatorPos = fdString.find(",");
        int readFd = stoi(fdString.substr(0, separatorPos));
        int writeFd = stoi(fdString.substr(separatorPos + 1));

        // Read the vector size from the pipe
        size_t vectorSize;
        read(readFd, &vectorSize, sizeof(vectorSize));

        // Read the vector elements from the pipe
        vector<float> values(vectorSize);
        read(readFd, values.data(), vectorSize * sizeof(float));

        // Assign the received vector to the Data class
        NeuralNetworkData neuralNetworkData("Neurons1.txt", "Weights1.txt");
        neuralNetworkData.neurons = values;

        // Calculate FX values
        vector<float> FX(2, 0.0);
        FX[0] = float(float(pow(values[0], 2) + values[0] + 1) / 2);
        FX[1] = float(float(pow(values[0], 2) - values[0]) / 2);
        cout << "FX1: " << FX[0] << " FX2: " << FX[1] << endl;

        pid_t p = fork();
        if (p == 0)
        {
            sleep(1);
            // Open the named pipe for writing
            int pipeFD = open(pipe_name, O_WRONLY);

            // Write the FX values to the named pipe
            for (const auto &value : FX)
            {
                write(pipeFD, &value, sizeof(float));
            }

            close(pipeFD);
        }
        sleep(1);
        return 1;
    }

    if (!executionFlag)
    {
        string fdString = argv[3];
        size_t separatorPos = fdString.find(",");
        int readFd = stoi(fdString.substr(0, separatorPos));
        int writeFd = stoi(fdString.substr(separatorPos + 1));

        // Read the vector size from the pipe
        size_t vectorSize;
        read(readFd, &vectorSize, sizeof(vectorSize));

        // Read the vector elements from the pipe
        vector<float> values(vectorSize);
        read(readFd, &values, vectorSize * sizeof(float));

        // Assign the received vector to the Data class
        NeuralNetworkData neuralNetworkData("Neurons1.txt", "Weights1.txt");
        neuralNetworkData.neurons = values;
    }

    vector<float> nextLayerNeurons(neuralNetworkData.weights[layerPosition].size(), 0.0);
    size_t vectorSize = nextLayerNeurons.size();

    // Write the vector size to the pipe
    write(pipeFD[1], &vectorSize, sizeof(vectorSize));
    // Write the vector elements to the pipe
    write(pipeFD[1], nextLayerNeurons.data(), vectorSize * sizeof(float));

    vector<pthread_t> threads(neuralNetworkData.neurons.size());

    for (int i = 0; i < neuralNetworkData.neurons.size(); ++i)
    {
        vector<float> weightsPerNeuron;

        for (int j = 0; j < neuralNetworkData.weights[layerPosition].size(); ++j)
        {
            weightsPerNeuron.push_back(neuralNetworkData.weights[layerPosition][j][i]);
        }

        ThreadArgs *threadArgs = new ThreadArgs(neuralNetworkData.weights, neuralNetworkData.neurons[i], i, pipeFD[0], pipeFD[1], layerPosition);
        pthread_create(&threads[i], nullptr, calculate, threadArgs);
        sleep(1);
    }
    sleep(neuralNetworkData.neurons.size() + 1);
    sem_destroy(&semaphore);

    // Read the vector size from the pipe
    size_t vectorSize2;
    read(pipeFD[0], &vectorSize2, sizeof(vectorSize2));

    // Read the vector elements from the pipe
    vector<float> vec2(vectorSize2);
    read(pipeFD[0], vec2.data(), vectorSize2 * sizeof(float));

    cout << "Position: " << layerPosition << endl;

    if (layerPosition <= LayerLength)
    {
        pipe(pipeFD);
        ssize_t vectorSize3 = vec2.size();
        write(pipeFD[1], &vectorSize3, sizeof(vectorSize3));
        write(pipeFD[1], vec2.data(), vec2.size() * sizeof(float));

        executionFlag = false;
        layerPosition += 1;
        neuralNetworkData.neurons = vec2;

        cout << "Entering Layer " << layerPosition << endl;

        char childStack[STACK_SIZE];
        int cloneFlags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD;
        pid_t pid = clone(childFunction, childStack + STACK_SIZE, cloneFlags, &pipeFD);
        waitpid(pid, nullptr, 0);

        cout << "Backtracking Layer " << layerPosition << endl;

        vector<float> FXX;
        float vall;
        pid_t p = fork();
        if (p == 0)
        {
            sleep(5);
            int pipeFD1 = open("pipe2", O_RDONLY);
            while (read(pipeFD1, &vall, sizeof(float)) > 0)
            {
                FXX.push_back(vall);
            }
            close(pipeFD1);

            // Read from pipe2 and write back to pipe1
            int pipeFD2 = open(pipe_name, O_WRONLY);

            for (int i = 0; i < FXX.size(); i++)
            {
                write(pipeFD2, &FXX[i], sizeof(float));
            }
            close(pipeFD2);
        }
        else
        {

            int pipeFD1 = open(pipe_name, O_RDONLY);
            while (read(pipeFD1, &vall, sizeof(float)) > 0)
            {
                FXX.push_back(vall);
            }
            close(pipeFD1);

            cout << "FX1: " << FXX[0] << std::endl;

            cout << "FX2: " << FXX[1] << std::endl;

            // Read from pipe2 and write back to pipe1
            int pipeFD2 = open("pipe2", O_WRONLY);

            for (int i = 0; i < FXX.size(); i++)
            {
                write(pipeFD2, &FXX[i], sizeof(float));
            }
            close(pipeFD2);
        }

        return 1;
    }

    return 0;
}

int childFunction(void *arg)
{
    int *pipeFD = static_cast<int *>(arg);
    string fdString = to_string(pipeFD[0]) + "," + to_string(pipeFD[1]);
    const char *newArgv[] = {"./run", "1", to_string(layerPosition).c_str(), fdString.c_str(), nullptr};

    execv(newArgv[0], const_cast<char *const *>(newArgv));
    perror("execv"); // If execv returns, there was an error.
    return 1;
}
