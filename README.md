# CASM: An Engine-Centric Adaptive Framework for Continuous Subgraph Matching

Subgraph matching is a fundamental operation for graph analytics, yet its continuous version over streaming graphs remains challenging due to the combinatorial explosion of the search space. Existing approaches primarily focus on incremental index maintenance or filtering techniques, often leaving the underlying backtracking search engines unoptimized and thus struggling to process high-frequency updates efficiently. In this paper, we propose CASM, an _engine-centric_ **C**ontinuous **A**daptive **S**ubgraph **M**atching framework. CASM mitigates the heavy maintenance overhead of structural states by introducing an Incremental Structural Equivalence Index (ISEI), which reduces complex topological comparisons to an efficient signature sorting process. To avoid performance degradation from dynamic anchor constraints and physical candidate splitting during streaming updates, CASM employs an Anchor-Driven Asymmetric Engine. Furthermore, an entropy-driven adaptive routing mechanism balances the pruning benefits with indexing costs by quantifying graph structural redundancy in real time, effectively preventing the over-indexing problem. Extensive experiments demonstrate that CASM consistently outperforms state-of-the-art baselines, achieving several orders of magnitude higher throughput, particularly on large-scale dense graphs.

## Compile

Under the root directory of the project, execute the following commands to compile the source code.

```zsh
mkdir build
cd build
cmake ..
make
```

## Correctness Verification

We use the test cases from [RapidFlow](https://github.com/shixuansun/RapidFlow). The usage is as follows:

```bash
python valid.py ../build/matching/BS
```

The test script will output whether each test case passes. Additionally, you can verify the correctness of our code by running your own datasets.

## Execute

After compiling the source code, you can find the binary file 'BS' under the 'build/matching' directory.
Execute the binary with the following command ./BS -d data_graph -q query_graph -u update_file -n max_number_of_embeddings -t max_execute_time -o ouput_file,
in which -d specifies the input of the data graphs, -q specifies the input of the query graphs, and -u specifies the input of the update stream.
The -n parameter sets the maximum number of embeddings that you would like to find. The -t parameter constrains the maximum execution time(ms). If the number of embeddings enumerated reaches the limit or all the results have been found or the time limit is reached, then the program will terminate.
Set -n as 'MAX' to find all results.

Example:

```zsh
./BS -d valid/delete/data_graph/data.graph -q valid/delete/query_graph/Q_1 -u valid/delete/data_graph/deletion.graph -t 300000 -n MAX
```

## Input

Both the input query graph and data graph are vertex-labeled.
Each graph starts with 't N M' where N is the number of vertices and M is the number of edges. A vertex and an edge are formatted
as 'v VertexID LabelId Degree' and 'e VertexId VertexId' respectively. Note that we require that the vertex
id is started from 0 and the range is [0,N - 1] where V is the vertex set. The following
is an input sample. You can also find sample data sets and query sets under the test folder.

Example:

```zsh
t 5 6
v 0 0 2
v 1 1 3
v 2 2 3
v 3 1 2
v 4 2 2
e 0 1
e 0 2
e 1 2
e 1 3
e 2 4
e 3 4
```

Each line of the update stream file contains a single edge update, formatted as `e/-e start_v_id end_v_id`.

Example:

```zsh
-e 814 1097
e 2164 2165
-e 1616 2066
e 1997 2214
e 306 2272
```

## Experiment Datasets

We have placed all the datasets used for testing in the paper at this link: [dataset_CASM](https://huggingface.co/datasets/Lu-Yujie/CASM-dataset).

```bash
7z x CASM.7z
```
