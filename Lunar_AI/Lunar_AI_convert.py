import torch
import torch.nn as nn
import numpy as np

N_LEVELS, N_FEATURES, LOG2_HASHMAP_SIZE = 20, 2, 21
BASE_RES_LAT, FINEST_RES_LAT = 16, 46080
PRIME_LON, PRIME_LAT = 1, 2654435761


class _Encoding(nn.Module):
    def __init__(self, recommended_lon_cells):
        super().__init__()
        b = np.exp((np.log(FINEST_RES_LAT) - np.log(BASE_RES_LAT)) / (N_LEVELS - 1))
        self.res_lat = [max(1, int(round(BASE_RES_LAT * (b ** l)))) for l in range(N_LEVELS)]
        aspect = recommended_lon_cells / FINEST_RES_LAT
        self.res_lon = [max(1, int(round(r * aspect))) for r in self.res_lat]
        max_size = 2 ** LOG2_HASHMAP_SIZE
        self.table_sizes, self.use_hash = [], []
        for rlon, rlat in zip(self.res_lon, self.res_lat):
            n = (rlon + 1) * (rlat + 1)
            self.table_sizes.append(min(n, max_size))
            self.use_hash.append(n > max_size)
        self.embeddings = nn.ModuleList([nn.Embedding(s, N_FEATURES) for s in self.table_sizes])

    def _idx(self, ix, iy, lvl):
        if not self.use_hash[lvl]:
            return ix * (self.res_lat[lvl] + 1) + iy
        h = (ix.to(torch.int64) * PRIME_LON) ^ (iy.to(torch.int64) * PRIME_LAT)
        return h % self.table_sizes[lvl]

    def forward(self, u, v):
        out = []
        for lvl in range(N_LEVELS):
            rlon, rlat = self.res_lon[lvl], self.res_lat[lvl]
            x, y = u * rlon, v * rlat
            x0f, y0f = torch.floor(x), torch.floor(y)
            fx, fy = (x - x0f).unsqueeze(-1), (y - y0f).unsqueeze(-1)
            x0, x1 = x0f.long().clamp(0, rlon), (x0f.long() + 1).clamp(0, rlon)
            y0, y1 = y0f.long().clamp(0, rlat), (y0f.long() + 1).clamp(0, rlat)
            e00 = self.embeddings[lvl](self._idx(x0, y0, lvl))
            e01 = self.embeddings[lvl](self._idx(x0, y1, lvl))
            e10 = self.embeddings[lvl](self._idx(x1, y0, lvl))
            e11 = self.embeddings[lvl](self._idx(x1, y1, lvl))
            top = e00 * (1 - fx) + e10 * fx
            bot = e01 * (1 - fx) + e11 * fx
            out.append(top * (1 - fy) + bot * fy)
        return torch.cat(out, dim=-1)


class LunarAltitudeNet(nn.Module):
    def __init__(self, recommended_lon_cells, hidden=256):
        super().__init__()
        self.encoding = _Encoding(recommended_lon_cells)
        self.mlp = nn.Sequential(
            nn.Linear(N_LEVELS * N_FEATURES, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, 1),
        )

    def forward(self, u, v):
        return self.mlp(self.encoding(u, v)).squeeze(-1)


def load(path, device="cpu"):
    """Returneaza (model, metadata_dict) gata de folosit."""
    ckpt = torch.load(path, map_location=device, weights_only=False)
    model = LunarAltitudeNet(ckpt["recommended_lon_cells"]).to(device)
    model.load_state_dict(ckpt["model_state"])
    model.eval().float()
    meta = {
        "alt_mean": ckpt["alt_mean"], "alt_std": ckpt["alt_std"],
        "limb_centers_deg": ckpt["limb_centers_deg"],
        "band_half_width_deg": ckpt["band_half_width_deg"],
        "lat_min": ckpt["lat_min"], "lat_max": ckpt["lat_max"],
    }
    return model, meta