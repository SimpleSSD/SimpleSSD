# SimpleSSD version 2.1
Open-Source Licensed Educational SSD Simulator for High-Performance Storage and Full-System Evaluations

This project is managed by [CAMELab](http://camelab.org).
For more information, please visit [SimpleSSD homepage](https://docs.simplessd.org).

## Licenses
SimpleSSD is released under the GPLv3 license. See `LICENSE` file for details.

SimpleSSD uses following open-source libraries:

### [pugixml](https://pugixml.org) located at `lib/pugixml`

XML parser library released under the MIT license.

```
This software is based on pugixml library (http://pugixml.org).
pugixml is Copyright (C) 2006-2018 Arseny Kapoulkine.
```

### [McPAT](https://github.com/HewlettPackard/mcpat) located at `/lib/mcpat`

Multicore Power Area and Timing calculator.
We modified source code to separate initialize phase and calculation phase.
