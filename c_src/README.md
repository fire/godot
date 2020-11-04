# Unifex Godot Engine

Launch.

```bash
iex -S mix
require Unifex.CNode
{:ok, pid} = Unifex.CNode.start_link(:godot)
Unifex.CNode.call(pid, :init, [["godot", "-v"]])
Unifex.CNode.call(pid, :iteration, [0.033])
Unifex.CNode.call(pid, :call, ["get_node_count"])
Unifex.CNode.call(pid, :call, ["get_method_list"])
Unifex.CNode.call(pid, :init, [["godot", "-v"]])
Unifex.CNode.stop(pid)
```