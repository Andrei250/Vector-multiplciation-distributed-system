#include <iostream>
#include <fstream>
#include "mpi.h"
#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

void readChildren(string filename, int rank,
                    vector<int>& innerCluster,
                    unordered_map<int, int>& parent) {
    ifstream f(filename);
    int N;

    f >> N;

    for (int i = 0; i < N; ++i) {
        int child;

        f >> child;
        innerCluster.push_back(child);
        parent[child] = rank;
    }

    f.close();
}

void sendToChildren(vector<int> innerConnection, int rank, vector<pair<int,int>> data) {
    int sz = data.size();

    for (auto it : innerConnection) {
        MPI_Send(&rank, 1, MPI_INT, it, 0, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
        MPI_Send(&sz, 1, MPI_INT, it, 0, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
        MPI_Send((void*)data.data(), sz * sizeof(pair<int, int>), MPI_BYTE, it, 0, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
    }
}

void printSystem(int rank, vector<pair<int, int>> elements) {
    string ans = to_string(rank) + " -> ";

    for (int i = 0; i < 3; ++i) {
        ans.push_back((i + '0'));
        ans.push_back(':');

        for (auto it : elements) {
            if (it.second == i) {
                ans += to_string(it.first) + ",";
            }
        }

        ans.pop_back();
        ans.push_back(' ');
    }

    cout << ans << '\n';
}

void secondClusterCoordConn(unordered_map<int, int>& parent, vector<int>& innerCone) {
    int numberOfElements;
    MPI_Status status;
    vector<int> elements;

    // Information from the first Cluster
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 0;
        }
    }
    
    // Information from the thirs Cluster
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 1, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 1;
        }
    }

    vector<pair<int, int>> mapPairs;
    int sz;

    for (auto it : parent) {
        mapPairs.push_back({it.first, it.second});
    }

    printSystem(2, mapPairs);
    sz = mapPairs.size();

    MPI_Send(&sz, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 0 << ")\n";
    MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 0 << ")\n";

    MPI_Send(&sz, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 1 << ")\n";
    MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 1 << ")\n";

    sendToChildren(innerCone, 2, mapPairs);
}

void clusterConn(unordered_map<int, int>& parent, vector<int> innerCluster, int rank) {
    int numberOfElements = innerCluster.size();
    vector<pair<int, int>> elements;
    MPI_Status status;

    MPI_Send(&numberOfElements, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << 2 << ")\n";
    MPI_Send((void*)innerCluster.data(), numberOfElements * sizeof(int), MPI_BYTE, 2, 0, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << 2 << ")\n";

    MPI_Recv(&numberOfElements, 1, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);
    elements = vector<pair<int,int>>(numberOfElements);
    MPI_Recv((void*)elements.data(), numberOfElements * sizeof(pair<int,int>), MPI_BYTE, 2, 0, MPI_COMM_WORLD, &status);

    for (auto it : elements) {
        parent[it.first] = it.second;
    }

    printSystem(rank, elements);
    sendToChildren(innerCluster, rank, elements);
}

void getSystemForChild(int rank, unordered_map<int, int>& parent, int& ancestor) {
    vector<pair<int, int>> elements;
    MPI_Status status;
    int numberOfElements;

    MPI_Recv(&ancestor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    MPI_Recv(&numberOfElements, 1, MPI_INT, ancestor, 0, MPI_COMM_WORLD, &status);
    elements = vector<pair<int,int>>(numberOfElements);
    MPI_Recv((void*)elements.data(), numberOfElements * sizeof(pair<int,int>), MPI_BYTE, ancestor, 0, MPI_COMM_WORLD, &status);

    for (auto it : elements) {
        parent[it.first] = it.second;
    }

    printSystem(rank, elements);
}

int numberOfChildren(int rank, unordered_map<int, int> mp) {
    int ans = 0;

    for (auto it : mp) {
        if (it.second == rank) {
            ans++;
        }
    }

    return ans;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Invalid usage\n";
        exit(1); 
    }

    int procs, rank;
    vector<int> innerCluster;
    unordered_map<int, int> parent;
    string filename;
    bool isConnected = atoi(argv[2]) == 1 ? true : false;
    int ancestor = -1;
    int arraySize = atoi(argv[1]);
    vector<int> arr(arraySize);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    filename = "cluster" + to_string(rank) + ".txt";

    if (rank < 3) {
        readChildren(filename, rank, innerCluster, parent);
    }

    parent[0] = parent[1] = parent[2] = -1;

    if (rank == 2) {
        secondClusterCoordConn(parent, innerCluster);
    } else if (rank < 3) {
        clusterConn(parent, innerCluster, rank);
    } else {
        getSystemForChild(rank, parent, ancestor);
    }

    if (rank == 0) {
        for (int i = 0; i < arraySize; ++i) {
            arr[i] = i;
        }
    } else if (rank < 3) {

    } else {
        
    }

    MPI_Finalize();
}