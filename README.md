# The ACG (Argument Call Graph) plugin to [MetaCG](https://github.com/tudasc/MetaCG) [CaGe](https://github.com/tudasc/MetaCG/tree/devel/tools/cage)
This plugin uses the CaGe tool to generate a call-graph at link-time of a given program and augments the generated graph with additional information about the flow of local variables along call edges.
It adds the md_locals and md_arg_flow metadata structs.
