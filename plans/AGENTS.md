# Plans Directory
Plans are used to provide AI agents with incremental implementation instructions. A good plan should, generally, focus on a single feature and result in small changes to the codebase.

Plans associated with the current release live in the top level directory. Before tagging a new release, plans are moved to the a directory with a name associated with the version number of the release. This provides an auditable archive of what instructions were given to the AI and when. Never delete a plan or its review document.

Treat a plan ID as the exact filename stem string, including all leading zeroes. Never convert it to a number, strip zeroes, or otherwise normalize it. For example, `0001` resolves to `plans/0001.md`, while `00010` resolves to `plans/00010.md`; all associated artifact names must use that same exact stem.

Currently plans are being executed in a sequential process. Plans are given numbers. A plan with a lower number than the one currently being worked can be assumed to be completed. This will change in the future as asyncronous workflows become possible.
