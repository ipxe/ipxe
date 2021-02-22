@Library("dst-shared@master") _
rpmBuild (
    githubPushRepo : "Cray-HPE/metal-ipxe",
    githubPushBranches : "release/.*|main",
    specfile : "metal-ipxe.spec",
    masterBranch : "main",
    product : "csm",
    target_node : "ncn",
    fanout_params : ["sle15sp2"],
    channel : "metal-ci-alerts",
    slack_notify : ['', 'SUCCESS', 'FAILURE', 'FIXED']
)
