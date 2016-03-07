ADPhotron XML files
===================

photronAttributes.xml
---------------------

This file contains information about the detector that should be recorded in data formats that support metadata (HDF5, netCDF).  Many of the items specified in this xml file are saved by Photron PFV software in .cih files.  This file is intended to be included in the "Attributes" section of Photron.adl, which causes the attributes to be available to all areaDetector plugins.

hdf5Attributes.xml
------------------

This file contains additional information that should be recorded, but isn't specific to the detector.  This file is intended to be included in the "Attributes" Section of NDFileHDF5.adl, which causes the attributes to be available only to the HDF5 plugin.

hdf5_exchange_layout.xml
------------------------

If no layout is specified in the HDF5 plugin, a default format that will be used that saves every attribute with every frame that is acquired.  Specifying a layout gives more control over where data resides in the data file and when the attributes are saved.  hdf5_exchange_layout.xml provides a starting point for a layout that is compatible with the data-exchange format.  For more information on the data-exchange format, check out the links below:

* [Data Exchange Basics](https://github.com/data-exchange/data-exchange/blob/master/xtomo/docs/DataExchangeBasics.pdf)

* [Data Exchange Reference](https://github.com/data-exchange/data-exchange/blob/master/xtomo/docs/DataExchangeReference.pdf)

* [Example Attributes & Layout XML files](https://github.com/data-exchange/data-exchange/tree/master/xtomo/docs/areadetector)

