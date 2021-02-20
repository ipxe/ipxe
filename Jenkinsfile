@Library("dst-shared@release/shasta-1.4") _
rpmBuild (
    specfile : "metal-ipxe.spec",
    masterBranch : "main",
    product : "csm",
    target_node : "ncn",
    fanout_params : ["sle15sp2"],
    channel : "metal-ci-alerts",
    slack_notify : ['', 'SUCCESS', 'FAILURE', 'FIXED']
)
