import flux
import argparse
import flux.job
import flux.job.JobID
from flux.job.JobID import id_parse


def getJobInfo(handle, jobId):
    jobId = id_parse(jobId)
    return flux.job.get_job(handle, jobId)


def getNodeList(nodeData):
    if "[" in nodeData:
        hostname, ranges = nodeData.strip().split("[")
        ranges = ranges.rstrip("]").split(",")

        hostList = []
        for range_ in ranges:
            if "-" in range_:
                start, end = map(int, range_.split("-"))
                hostList.extend(f"{hostname}{i}" for i in range(start, end + 1))
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
    hostList = getNodeList(jobInfo["nodelist"])
    try:
        startTime = getJobStartTime(jobInfo)
        endTime = getjobEndTime(jobInfo)
        if startTime == 0 or endTime == 0:
            raise Exception
    except:
        print("Issue in getting time value")
        return
    print(
        f"making an RPC call for start_time: {startTime}, end_time {endTime} and nodeList {hostList}"
    )
    print(
        h.rpc(
            "flux_pwr_monitor.get_node_power",
            {
                "start_time": startTime,
                "end_time": endTime,
                "nodelist": hostList,
            },
            nodeid=0,
            flags=flux.constants.FLUX_RPC_STREAMING,
        ).get()
    )
    # print(
    #     h.rpc(
    #         "flux_pwr_monitor.get_node_power",
    #         {"start_time": 0, "end_time": 10, "nodelist": ["tioga23"]},
    #         nodeid=0,
    #         flags=flux.constants.FLUX_RPC_STREAMING,
    #     ).get()
    # )
    # print(
    #     h.rpc(
    #         "flux_pwr_monitor.get_node_power",
    #         {"start_time": 0, "end_time": 10, "nodelist": ["tioga30"]},
    #         nodeid=0,
    #         flags=flux.constants.FLUX_RPC_STREAMING,
    #     ).get()
    # )
    #
    # print(
    #     h.rpc(
    #         "flux_pwr_monitor.get_node_power",
    #         {"start_time": 0, "end_time": 10, "nodelist": ["tioga31"]},
    #         nodeid=0,
    #         flags=flux.constants.FLUX_RPC_STREAMING,
    #     ).get()
    # )
    # print(
    #     h.rpc(
    #         "flux_pwr_monitor.get_node_power",
    #         {"start_time": 0, "end_time": 10, "nodelist": ["tioga32"]},
    #         nodeid=0,
    #         flags=flux.constants.FLUX_RPC_STREAMING,
    #     ).get()
    # )
    #
    # print(json.dumps(flux.Flux().rpc("overlay.topology", {"rank": 0}).get()))


if __name__ == "__main__":
    main()
