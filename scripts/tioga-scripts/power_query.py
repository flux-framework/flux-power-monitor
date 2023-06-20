import flux
import argparse


def getJobInfo(jobId):
    return flux.list.get_job(jobId)


def main():
    parser = argparse.ArgumentParser(description="Client for flux_pwr_monitor")
    parser.add_argument("-j", type=str, default=0, help="Flux JobId")
    args = parser.parse_args()
    jobId = args.j
    h = flux.Flux()
    # jobInfo = getJobInfo(jobId)
    # print(jobInfo)
    print("making an RPC call for getting data")
    print(
        h.rpc(
            "flux_pwr_monitor.get_node_power",
            {"start_time": 0, "end_time": 10, "nodelist": ["tioga23"]},
            nodeid=0,
            flags=flux.constants.FLUX_RPC_STREAMING,
        ).get()
    )


if __name__ == "__main__":
    main()
