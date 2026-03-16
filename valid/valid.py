"""
Execute binary with argument. If the argument is pattern, then start it with $.
"""

from subprocess import Popen, PIPE
import sys
import os
import glob


def execute_binary(args):
    process = Popen(' '.join(args), shell=True, stdout=PIPE, stderr=PIPE)
    (std_output, std_error) = process.communicate()
    process.wait()
    rc = process.returncode

    return rc, std_output, std_error

# generate execution arguments
def generate_args(binary, *params):
    arguments = [binary]
    arguments.extend(list(params))
    return arguments


def check_correctness(binary_path, data_graph_path, data_graph_update_path, query_folder_path, expected_results):
    # find all query graphs.
    query_graph_path_list = glob.glob('{0}/*'.format(query_folder_path))
    
    # List to store failed cases: tuples of (query_name, expected, actual)
    failed_cases = []

    for query_graph_path in query_graph_path_list:
        execution_args = generate_args(binary_path, '-d', data_graph_path, '-q', query_graph_path,
                                       '-u', data_graph_update_path, '-n', 'MAX')

        (rc, std_output, std_error) = execute_binary(execution_args)
        query_graph_name = os.path.splitext(os.path.basename(query_graph_path))[0]

        if rc == 0:
            embedding_num = 0
            # Decode bytes to string if necessary (Python 3 compatibility)
            if isinstance(std_output, bytes):
                std_output = std_output.decode('utf-8')
            
            std_output_list = std_output.split('\n')
            for line in std_output_list:
                if '#Total Embeddings' in line:
                    embedding_num = int(line.split(':')[1].strip())
                    break

            expected_embedding_num = expected_results.get(query_graph_name)
            
            if expected_embedding_num is None:
                print('Warning: No expected result found for {0}'.format(query_graph_name))
                continue

            if embedding_num != expected_embedding_num:
                # Instead of exit, record the failure and print a message
                print('--> FAILED: {0}. Expected {1}, Output {2}'.format(query_graph_name, expected_embedding_num,
                                                                  embedding_num))
                failed_cases.append({
                    'name': query_graph_name,
                    'expected': expected_embedding_num,
                    'actual': embedding_num,
                    'type': 'Mismatch'
                })
            else:
                print('pass {0}:{1}'.format(query_graph_name, embedding_num))
        else:
            # Handle execution error
            print('--> ERROR: Query {0} execution error.'.format(query_graph_name))
            failed_cases.append({
                    'name': query_graph_name,
                    'expected': 'N/A',
                    'actual': 'Execution Error (rc={0})'.format(rc),
                    'type': 'Runtime Error'
                })
    
    return failed_cases


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python script.py <binary_path>')
        exit(-1)

    input_binary_path = sys.argv[1]
    if not os.path.isfile(input_binary_path):
        print('The binary {0} does not exist.'.format(input_binary_path))
        exit(-1)

    # load expected results.
    dir_path = os.path.dirname(os.path.realpath(__file__))
    
    # Container for all failures across different test suites
    all_failures = []

    # --- Test Suite 1: Insert ---
    print("--------------------------------------------------")
    print("Running Insert Test Cases...")
    input_data_graph_path = '{0}/insert/data_graph/data.graph'.format(dir_path)
    input_data_graph_update_path = '{0}/insert/data_graph/insertion.graph'.format(dir_path)
    input_query_graph_folder_path = '{0}/insert/query_graph/'.format(dir_path)
    input_expected_results_file = '{0}/insert/expected_output.txt'.format(dir_path)

    input_expected_results = {}
    if os.path.exists(input_expected_results_file):
        with open(input_expected_results_file, 'r') as f:
            for line in f:
                if line:
                    result_item = line.split(':')
                    if len(result_item) >= 2:
                        input_expected_results[result_item[0].strip()] = int(result_item[1].strip())

        insert_failures = check_correctness(input_binary_path, input_data_graph_path, input_data_graph_update_path,
                                            input_query_graph_folder_path, input_expected_results)
        all_failures.extend(insert_failures)
    else:
        print("Expected results file not found: {0}".format(input_expected_results_file))

    # --- Test Suite 2: Delete ---
    print("\n--------------------------------------------------")
    print("Running Delete Test Cases...")
    input_data_graph_path = '{0}/delete/data_graph/data.graph'.format(dir_path)
    input_data_graph_update_path = '{0}/delete/data_graph/deletion.graph'.format(dir_path)
    input_query_graph_folder_path = '{0}/delete/query_graph/'.format(dir_path)
    input_expected_results_file = '{0}/delete/expected_output.txt'.format(dir_path)

    input_expected_results = {}
    if os.path.exists(input_expected_results_file):
        with open(input_expected_results_file, 'r') as f:
            for line in f:
                if line:
                    result_item = line.split(':')
                    if len(result_item) >= 2:
                        input_expected_results[result_item[0].strip()] = int(result_item[1].strip())

        delete_failures = check_correctness(input_binary_path, input_data_graph_path, input_data_graph_update_path,
                                            input_query_graph_folder_path, input_expected_results)
        all_failures.extend(delete_failures)
    else:
        print("Expected results file not found: {0}".format(input_expected_results_file))

    # --- Final Summary ---
    print("\n==================================================")
    print("Final Summary")
    print("==================================================")
    
    if len(all_failures) == 0:
        print("SUCCESS: All test cases passed.")
        exit(0)
    else:
        print("FAILURE: {0} test cases failed.".format(len(all_failures)))
        print("{0:<20} | {1:<15} | {2:<20}".format("Query Name", "Expected", "Actual"))
        print("-" * 60)
        for fail in all_failures:
            print("{0:<20} | {1:<15} | {2:<20}".format(fail['name'], str(fail['expected']), str(fail['actual'])))
        exit(-1)