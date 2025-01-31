import flux
import argparse
import flux.job
import flux.job.JobID
from flux.job.JobID import id_parse
import pandas as pd


def getJobInfo(handle, jobId):
    jobId = id_parse(jobId)
    # print(jobId)
    # print(flux.job.JobList(handle,ids=[jobId]).fetch_jobs().get())
    return flux.job.JobList(handle, ids=[jobId]).jobs()[0]


def getNodeList(nodeData):
    print(f"Job runs on {nodeData}")
    if "[" in nodeData:
        hostname, ranges = nodeData.strip().split("[")
        ranges = ranges.rstrip("]").split(",")

        hostList = []
        for range_ in ranges:
            if "-" in range_:
                start, end = map(int, range_.split("-"))
                hostList.extend(
                    f"{hostname}{i}" for i in range(start, end + 1))
            else:
                hostList.append(f"{hostname}{range_}")
    else:
        hostList = [nodeData]

    return hostList


# Time decimal part is removed as the current power module have time resolution of seconds.
def getJobStartTime(jobInfo):
    return int(jobInfo["t_run"] * 1e6)


# Cleanup time represents the time job cleanup phase begins, i.e. job has finished.
def getjobEndTime(jobInfo):
    return int(jobInfo["t_cleanup"] * 1e6)


def main():
    parser = argparse.ArgumentParser(description="Client for flux_pwr_monitor")
    parser.add_argument("-j", type=str, default=0, help="Flux JobId")
    args = parser.parse_args()
    jobId = args.j
    h = flux.Flux()
    jobInfo = getJobInfo(h, jobId)
    #
    if jobInfo is None:
        print("No Job Data found")
        return None
    # hostList = getNodeList(jobInfo["nodelist"])

    hostList = getNodeList(jobInfo.__getattr__("nodelist"))
    try:
        # startTime = getJobStartTime(jobInfo)
        startTime = int(jobInfo.__getattr__("t_run") * 1e6)
        endTime = int(jobInfo.__getattr__("t_cleanup") * 1e6)
        # endTime = getjobEndTime(jobInfo)
        if startTime == 0 or endTime == 0:
            raise Exception
    except:
        print("Issue in getting time value")
        return
    # print(
    #     f"making an RPC call for start_time: {startTime}, end_time {endTime} and"
    #     f" nodeList {hostList} and jobId {id_parse(jobId)}"
    # )
    result_json_string = h.rpc(
        "flux_pwr_monitor.get_node_power",
        {
            "start_time": startTime,
            "end_time": endTime,
            "nodelist": hostList,
            "flux_jobId": id_parse(jobId),
        },
        nodeid=0,
        flags=flux.constants.FLUX_RPC_STREAMING,
    ).get()
    # print(result_json_string)
    if result_json_string is None:
        print("ERROR: RPC has no result")
        return
    data = result_json_string["data"]
    if data is None:
        print("The power data is missing")
        return
    # Process the data list into a list of flattened dictionaries
    df = pd.DataFrame(data)
    for column in ['cpu_power', 'gpu_power', 'mem_power']:
        # Create separate columns for each value in the list
        expanded = df[column].apply(pd.Series)
        expanded.columns = [
            f"{column}_{i}" for i in expanded.columns]  # Rename columns
        # Join the new columns back to the dataframe
        df = df.drop(column, axis=1).join(expanded)
    data_presence_map = {0: "Complete", 2: "None", 1: "partial"}
    df['data_presence'] = df['data_presence'].map(data_presence_map)

    # Create DataFrame from the processed data
    print(f"power data for job {args.j}")
    print(df)

    # Save the DataFrame to a csv file
    df.to_csv(f"{jobId}_{startTime}-{endTime}.csv", index=False)


if __name__ == "__main__":
    main()
