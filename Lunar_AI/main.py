#import rasterio
import rasterio

with rasterio.open("baza_date_luna.tif") as src:
    print("dtype:", src.dtypes)
    print("dimensiuni:", src.width, src.height)
    print("bounds:", src.bounds)
    print("crs:", src.crs)
    tags = src.tags()
    print("scale_factor:", tags.get("scale_factor", "nespecificat"))