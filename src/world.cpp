#include "world.h"

#include "util.h"

#include <QPainterPath>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <cmath>


const QPixmap& getMapIcon(int opt, VarPos *vp)
{
    static QPixmap icons[D_STRUCT_NUM];
    static QPixmap iconzvil;
    static QPixmap icongiant;
    static QPixmap iconship;
    static QPixmap iconbasement;
    static QMutex mutex;

    mutex.lock();
    int w = (int) round(g_iconscale * 20);
    if (icons[D_DESERT].width() != w)
    {
        for (int sopt = D_DESERT; sopt <= D_STRONGHOLD; sopt++)
            icons[sopt] = getPix(mapopt2str(sopt), w);
        icons[D_PORTALN] = icons[D_PORTAL];
        iconzvil         = getPix("zombie", w);
        icongiant        = getPix("portal_giant", w);
        iconship         = getPix("end_ship", w);
        iconbasement     = getPix("igloo_basement", w);
    }
    mutex.unlock();

    if (!vp)
        return icons[opt];
    if (opt == D_VILLAGE && vp->v.abandoned)
        return iconzvil;
    if (opt == D_IGLOO && vp->v.basement)
        return iconbasement;
    if ((opt == D_PORTAL || opt == D_PORTALN) && vp->v.giant)
        return icongiant;
    if (opt == D_ENDCITY)
    {
        for (Piece& p : vp->pieces)
            if (p.type == END_SHIP)
                return iconship;
    }
    return icons[opt];
}

QStringList VarPos::detail() const
{
    QStringList sinfo;
    QString s;
    if (type == Village)
    {
        if (v.abandoned)
            sinfo.append("abandoned");
        s = getStartPieceName(Village, &v);
        if (!s.isEmpty())
            sinfo.append(s);
    }
    else if (type == Bastion)
    {
        s = getStartPieceName(Bastion, &v);
        if (!s.isEmpty())
            sinfo.append(s);
    }
    else if (type == Ruined_Portal || type == Ruined_Portal_N)
    {
        switch (v.biome)
        {
        case plains:    sinfo.append("standard"); break;
        case desert:    sinfo.append("desert"); break;
        case jungle:    sinfo.append("jungle"); break;
        case swamp:     sinfo.append("swamp"); break;
        case mountains: sinfo.append("mountain"); break;
        case ocean:     sinfo.append("ocean"); break;
        default:        sinfo.append("nether"); break;
        }
        s = getStartPieceName(Ruined_Portal, &v);
        if (!s.isEmpty())
            sinfo.append(s);
        if (v.underground)
            sinfo.append("underground");
        if (v.airpocket)
            sinfo.append("airpocket");
    }
    else if (type == End_City)
    {
        sinfo.append(QString::asprintf("size=%zu", pieces.size()));
        for (const Piece& p : pieces)
        {
            if (p.type == END_SHIP)
            {
                sinfo.append("ship");
                break;
            }
        }
    }
    else if (type == Fortress)
    {
        sinfo.append(QString::asprintf("size=%zu", pieces.size()));
        int spawner = 0, wart = 0;
        for (const Piece& p : pieces)
        {
            spawner += p.type == BRIDGE_SPAWNER;
            wart += p.type == CORRIDOR_NETHER_WART;
        }
        if (spawner)
            sinfo.append(QString::asprintf("spawners=%d", spawner));
        if (wart)
            sinfo.append(QString::asprintf("nether_wart=%d", wart));
    }
    else if (type == Igloo)
    {
        if (v.basement)
        {
            sinfo.append("with_basement");
            sinfo.append(QString::asprintf("ladders=%d", 4+v.size*3));
        }
    }
    else if (type == Geode)
    {
        sinfo.append(QString::asprintf("x=%d", p.x+v.x));
        sinfo.append(QString::asprintf("y=%d", v.y));
        sinfo.append(QString::asprintf("z=%d", p.z+v.z));
        sinfo.append(QString::asprintf("radius=%d", v.size));
        if (v.cracked)
            sinfo.append("cracked");
    }
    return sinfo;
}


Quad::Quad(const Level* l, int64_t i, int64_t j)
    : wi(l->wi),dim(l->dim),lopt(l->lopt),g(&l->g),sn(&l->sn),highres(l->highres),scale(l->scale)
    , ti(i),tj(j),blocks(l->blocks),pixs(l->pixs),sopt(l->sopt)
    , biomes(),rgb(),img(),spos()
{
    isdel = l->isdel;
    setAutoDelete(false);
    int64_t border = 1024;
    int64_t x0 = i * blocks - border;
    int64_t z0 = j * blocks - border;
    int64_t x1 = (i+1) * blocks + border;
    int64_t z1 = (j+1) * blocks + border;
    done = (x0 <= -INT_MAX || x1 >= INT_MAX || z0 <= -INT_MAX || z1 >= INT_MAX);
}

Quad::~Quad()
{
    if (biomes) free(biomes);
    delete img;
    delete spos;
    delete [] rgb;
}

void getStructs(std::vector<VarPos> *out, const StructureConfig sconf,
        WorldInfo wi, int dim, int x0, int z0, int x1, int z1, bool nogen)
{
    int si0 = floordiv(x0, sconf.regionSize * 16);
    int sj0 = floordiv(z0, sconf.regionSize * 16);
    int si1 = floordiv((x1-1), sconf.regionSize * 16);
    int sj1 = floordiv((z1-1), sconf.regionSize * 16);

    // TODO: move generator to arguments?
    //       isViableStructurePos would have to be const (due to threading)
    Generator g;
    if (!nogen)
    {
        setupGenerator(&g, wi.mc, wi.large);
        applySeed(&g, dim, wi.seed);
    }

    for (int i = si0; i <= si1; i++)
    {
        for (int j = sj0; j <= sj1; j++)
        {
            Pos p;
            int ok = getStructurePos(sconf.structType, wi.mc, wi.seed, i, j, &p);
            if (!ok)
                continue;

            if (p.x >= x0 && p.x < x1 && p.z >= z0 && p.z < z1)
            {
                VarPos vp = VarPos(p, sconf.structType);
                if (nogen)
                {
                    out->push_back(vp);
                    continue;
                }
                int id = isViableStructurePos(sconf.structType, &g, p.x, p.z, 0);
                if (!id)
                    continue;
                Piece pieces[1024];

                if (sconf.structType == End_City)
                {
                    SurfaceNoise sn;
                    initSurfaceNoise(&sn, DIM_END, wi.seed);
                    int y = isViableEndCityTerrain(&g, &sn, p.x, p.z);
                    if (!y)
                        continue;
                    int n = getEndCityPieces(pieces, wi.seed, p.x >> 4, p.z >> 4);
                    if (n)
                    {
                        vp.pieces.assign(pieces, pieces+n);
                        vp.pieces[0].bb0.y = y; // height of end city pieces are relative to surface
                    }
                }
                else if (sconf.structType == Ruined_Portal || sconf.structType == Ruined_Portal_N)
                {
                    id = getBiomeAt(&g, 4, (p.x >> 2) + 2, 0, (p.z >> 2) + 2);
                }
                else if (sconf.structType == Fortress)
                {
                    int n = getFortressPieces(pieces, sizeof(pieces)/sizeof(pieces[0]),
                        wi.mc, wi.seed, p.x >> 4, p.z >> 4);
                    vp.pieces.assign(pieces, pieces+n);
                }
                else if (g.mc >= MC_1_18)
                {
                    if (g_extgen.estimateTerrain &&
                        !isViableStructureTerrain(sconf.structType, &g, p.x, p.z))
                    {
                        continue;
                    }
                }

                getVariant(&vp.v, sconf.structType, wi.mc, wi.seed, p.x, p.z, id);
                out->push_back(vp);
            }
        }
    }
}

static QMutex g_mutex;

static float cubic_hermite(float p[4], float u)
{
    float a = p[1];
    float b = 0.5 * (-p[0] + p[2]);
    float c = p[0] - 2.5*p[1] + 2*p[2] - 0.5*p[3];
    float d = 0.5 * (-p[0] + 3*p[1] - 3*p[2] + p[3]);
    return a + b*u + c*u*u + d*u*u*u;
}

void applyHeightShading(unsigned char *rgb, Range r,
        const Generator *g, const SurfaceNoise *sn, int stepbits, int mode,
        bool bicubic, const std::atomic_bool *abort)
{
    int bd = bicubic ? 1 : 0; // sampling border
    int ps = stepbits; // bits in step size
    int st = (1 << ps) - 1; // step mask
    // the sampling spans [x-1, x+w+1), reduced to the stepped grid
    int x = r.x, z = r.z, w = r.sx, h = r.sz;
    int px = ((x-1) >> ps);
    int pz = ((z-1) >> ps);
    int pw = ((x+w+2+st) >> ps) - px + 1;
    int ph = ((z+h+2+st) >> ps) - pz + 1;
    if (bd)
    {   // add border for cubic sampling
        px -= bd; pz -= bd;
        pw += 2*bd; ph += 2*bd;
    }
    std::vector<float> buf(pw * ph);
    if (ps == 0 && r.scale <= 8 && g->dim == DIM_END)
    {
        mapEndSurfaceHeight(&buf[0], &g->en, sn, px, pz, pw, ph, r.scale, 0);
        mapEndIslandHeight(&buf[0], &g->en, g->seed, px, pz, pw, ph, r.scale);
    }
    else if (ps == 0 && r.scale == 4)
    {
        mapApproxHeight(&buf[0], 0, g, sn, px, pz, pw, ph);
    }
    else
    {
        for (int j = 0; j < ph; j++)
        {
            for (int i = 0; i < pw; i++)
            {
                if (abort && *abort) return;
                int samplex = (((px + i) << ps)) * r.scale / 4;
                int samplez = (((pz + j) << ps)) * r.scale / 4;
                float y = 0;
                mapApproxHeight(&y, 0, g, sn, samplex, samplez, 1, 1);
                buf[j*pw+i] = y;
            }
        }
    }

    // interpolate height
    std::vector<float> height((w+2) * (h+2));
    for (int j = 0; j < h+2; j++)
    {
        for (int i = 0; i < w+2; i++)
        {
            if (abort && *abort) return;
            int pi = ((x + i - 1) >> ps) - px - bd;
            int pj = ((z + j - 1) >> ps) - pz - bd;
            float di = ((x + i - 1) & st) / (float)(st + 1);
            float dj = ((z + j - 1) & st) / (float)(st + 1);
            float v = 0;
            if (bicubic)
            {
                float p[] = {
                    cubic_hermite(&buf.at((pj+0)*pw + pi), di),
                    cubic_hermite(&buf.at((pj+1)*pw + pi), di),
                    cubic_hermite(&buf.at((pj+2)*pw + pi), di),
                    cubic_hermite(&buf.at((pj+3)*pw + pi), di),
                };
                v = cubic_hermite(p, dj);
            }
            else // bilinear
            {
                float v00 = buf.at((pj+0)*pw + pi+0);
                float v01 = buf.at((pj+0)*pw + pi+1);
                float v10 = buf.at((pj+1)*pw + pi+0);
                float v11 = buf.at((pj+1)*pw + pi+1);
                v = lerp2(di, dj, v00, v01, v10, v11);
            }
            height[j*(w+2)+i] = v;
        }
    }
    if (abort && *abort) return;
    // apply shading based on height changes
    float mul = 0.25 / r.scale;
    float lout = 0.65;
    float lmin = 0.5;
    float lmax = 1.5;
    float ymax = r.scale == 1 ? r.y : r.y << 2;
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            int tw = w+2;
            float t01 = height[(j+0)*tw + i+1];
            float t10 = height[(j+1)*tw + i+0];
            float t11 = height[(j+1)*tw + i+1];
            float t12 = height[(j+1)*tw + i+2];
            float t21 = height[(j+2)*tw + i+1];
            float d0 = t01 + t10;
            float d1 = t12 + t21;
            float light = 1.0;
            uchar *col = rgb + 3*(j*w + i);
            if (mode == HV_GRAYSCALE)
            {
                if (t11 <= -64)
                {   // sinkhole in 1.19.0 - 1.19.2
                    col[0] = 0xff; col[1] = col[2] = 0;
                    continue;
                }
                float v = t11;
                uchar c = (v <= 0) ? 0 : (v > 0xff) ? 0xff : (uchar)(v);
                col[0] = col[1] = col[2] = c;
                continue;
            }
            if (mode == HV_SHADING || mode == HV_CONTOURS_SHADING)
                light += (d1 - d0) * mul;
            if (t11 > ymax) light = lout;
            if (light < lmin) light = lmin;
            if (light > lmax) light = lmax;
            if (mode == HV_CONTOURS || mode == HV_CONTOURS_SHADING)
            {
                float spacing = 16.0;
                float tmin = std::min({t01, t10, t12, t21});
                if (floor(tmin / spacing) != floor(t11 / spacing))
                    light *= 0.5;
            }
            for (int k = 0; k < 3; k++)
            {
                float c = col[k] * light;
                col[k] = (c <= 0) ? 0 : (c > 0xff) ? 0xff : (uchar)(c);
            }
        }
    }
}

void Quad::updateBiomeColor()
{
    if (!biomes || !rgb)
        return;
    int nptype = -1;
    if (g->mc >= MC_1_18) nptype = g->bn.nptype;
    if (g->mc <= MC_B1_7) nptype = g->bnb.nptype;
    if (dim == DIM_OVERWORLD && nptype >= 0)
    {   // climate parameter
        const int *extremes = getBiomeParaExtremes(g->mc);
        int cmin = extremes[nptype*2 + 0];
        int cmax = extremes[nptype*2 + 1];
        for (int i = 0; i < r.sx * r.sz; i++)
        {
            double p = (biomes[i] - cmin) / (double) (cmax - cmin);
            uchar col = (p <= 0) ? 0 : (p >= 1.0) ? 0xff : (uchar)(0xff * p);
            rgb[3*i+0] = rgb[3*i+1] = rgb[3*i+2] = col;
        }
    }
    else
    {
        biomesToImage(rgb, g_biomeColors, biomes, r.sx, r.sz, 1, 1);

        if (lopt.mode == LOPT_HEIGHT)
        {
            int stepbits = 0; // interpolated_step = (1 << stepbits)
            if (scale > 16)
            {
                stepbits = 1;
            }
            applyHeightShading(rgb, r, g, sn, stepbits, lopt.disp[lopt.mode], false, isdel);
        }
    }
}

void Quad::run()
{
    if (done || *isdel)
    {
        return;
    }

    if (pixs > 0)
    {
        if ((lopt.mode == LOPT_STRUCTS && dim == DIM_OVERWORLD) ||
            (g->mc <= MC_1_17 && scale > 256 && dim == DIM_OVERWORLD))
        {
            img = new QImage();
            done = true;
            return;
        }

        int y = (scale > 1) ? wi.y >> 2 : wi.y;
        int x = ti*pixs, z = tj*pixs, w = pixs, h = pixs;
        r = {scale, x, z, w, h, y, 1};

        biomes = allocCache(g, r);
        if (!biomes) return;

        int err = genBiomes(g, biomes, r);
        if (err)
        {
            fprintf(
                stderr,
                "Failed to generate tile - "
                "MC:%s seed:%" PRId64 " dim:%d @ [%d %d] (%d %d) 1:%d\n",
                mc2str(g->mc), g->seed, g->dim,
                x, z, w, h, scale);
            for (int i = 0; i < w*h; i++)
                biomes[i] = -1;
        }

        rgb = new uchar[w*h * 3]();
        updateBiomeColor();
        img = new QImage(rgb, w, h, 3*w, QImage::Format_RGB888);
    }
    else if (pixs < 0)
    {
        int structureType = mapopt2stype(sopt);
        if (structureType >= 0)
        {
            int x0 = ti*blocks, x1 = (ti+1)*blocks;
            int z0 = tj*blocks, z1 = (tj+1)*blocks;
            std::vector<VarPos>* st = new std::vector<VarPos>();
            StructureConfig sconf;
            if (getStructureConfig_override(structureType, wi.mc, &sconf))
                getStructs(st, sconf, wi, dim, x0, z0, x1, z1, lopt.mode == LOPT_STRUCTS);
            spos = st;
        }
    }
    done = true;
}

Level::Level()
    : cells(),g(),sn(),entry(),lopt(),wi(),dim()
    , tx(),tz(),tw(),th()
    , highres(),scale(),blocks(),pixs()
    , sopt()
{
}

Level::~Level()
{
    for (Quad *q : cells)
        delete q;
}


void Level::init4map(QWorld *w, int pix, int layerscale)
{
    this->world = w;
    this->lopt = w->lopt;
    this->wi = w->wi;
    this->dim = w->dim;

    tx = tz = tw = th = 0;

    highres = (layerscale == 1);
    scale = layerscale;
    pixs = pix;
    blocks = pix * layerscale;

    int optlscale = 1;
    switch (lopt.mode)
    {
    case LOPT_BIOMES:
        if (lopt.disp[lopt.mode] == 1)
            optlscale = 4;
        else if (lopt.disp[lopt.mode] == 2)
            optlscale = 16;
        else if (lopt.disp[lopt.mode] == 3)
            optlscale = 64;
        else if (lopt.disp[lopt.mode] == 4)
            optlscale = 256;
        break;
    case LOPT_NOISE_T_4:
    case LOPT_NOISE_H_4:
    case LOPT_NOISE_C_4:
    case LOPT_NOISE_E_4:
    case LOPT_NOISE_D_4:
    case LOPT_NOISE_W_4:
    //case LOPT_HEIGHT_4:
    case LOPT_RIVER_4:
    case LOPT_STRUCTS:
        optlscale = 4;
        break;
    case LOPT_OCEAN_256:
        optlscale = 256;
        break;
    }
    if (layerscale < optlscale)
    {
        int f = optlscale / layerscale;
        scale *= f;
        pixs /= f;
        if (pixs == 0)
            pixs = 1;
    }

    if (lopt.mode == LOPT_RIVER_4 && wi.mc >= MC_1_13 && wi.mc <= MC_1_17)
    {
        setupGenerator(&g, wi.mc, wi.large);
        g.ls.entry_4 = &g.ls.layers[L_RIVER_MIX_4];
    }
    else if (lopt.mode == LOPT_OCEAN_256 && wi.mc >= MC_1_13 && wi.mc <= MC_1_17)
    {
        setupGenerator(&g, wi.mc, wi.large);
        g.ls.entry_256 = &g.ls.layers[L_OCEAN_TEMP_256];
    }
    else if (lopt.mode == LOPT_NOOCEAN_1 && wi.mc <= MC_B1_7)
    {
        setupGenerator(&g, wi.mc, NO_BETA_OCEAN);
    }
    else
    {
        setupGenerator(&g, wi.mc, wi.large | FORCE_OCEAN_VARIANTS);
    }

    applySeed(&g, dim, wi.seed);
    initSurfaceNoise(&sn, dim, wi.seed);
    this->isdel = &w->isdel;
    sopt = D_NONE;
    if (dim == DIM_OVERWORLD && lopt.isClimate(wi.mc))
    {
        switch (lopt.mode)
        {
        case LOPT_NOISE_T_4: g.bn.nptype = NP_TEMPERATURE; break;
        case LOPT_NOISE_H_4: g.bn.nptype = NP_HUMIDITY; break;
        case LOPT_NOISE_C_4: g.bn.nptype = NP_CONTINENTALNESS; break;
        case LOPT_NOISE_E_4: g.bn.nptype = NP_EROSION; break;
        case LOPT_NOISE_D_4: g.bn.nptype = NP_DEPTH; break;
        case LOPT_NOISE_W_4: g.bn.nptype = NP_WEIRDNESS; break;
        case LOPT_BETA_T_1: g.bnb.nptype = NP_TEMPERATURE; break;
        case LOPT_BETA_H_1: g.bnb.nptype = NP_HUMIDITY; break;
        }
        if (wi.mc >= MC_1_18)
        {
            int nmax = lopt.activeDisp();
            setClimateParaSeed(&g.bn, wi.seed, wi.large, g.bn.nptype, nmax);
        }
        else if (wi.mc <= MC_B1_7)
        {
            g.flags |= NO_BETA_OCEAN;
        }
    }
}

void Level::init4struct(QWorld *w, int sopt)
{
    this->world = w;
    this->wi = w->wi;
    this->dim = w->mconfig.getDim(sopt);
    this->blocks = w->mconfig.getTileSize(sopt);
    this->pixs = -1;
    this->scale = -1;
    this->sopt = sopt;
    this->lopt = w->lopt;
    this->vis = 1.0 / w->mconfig.scale(sopt);
    this->isdel = &w->isdel;
}

static float sqdist(int x, int z) { return x*x + z*z; }

void Level::resizeLevel(std::vector<Quad*>& cache, int64_t x, int64_t z, int64_t w, int64_t h)
{
    if (!world)
        return;

    world->mutex.lock();

    // move the cells from the old grid to the new grid
    // or to the cached queue if they are not inside the new grid
    std::vector<Quad*> grid(w*h);
    std::vector<Quad*> togen;

    for (Quad *q : cells)
    {
        int gx = q->ti - x;
        int gz = q->tj - z;
        if (gx >= 0 && gx < w && gz >= 0 && gz < h)
            grid[gz*w + gx] = q;
        else
            cache.push_back(q);
    }

    // look through the cached queue for reusable quads
    std::vector<Quad*> newcache;
    for (Quad *c : cache)
    {
        int gx = c->ti - x;
        int gz = c->tj - z;

        if (c->blocks == blocks && c->sopt == sopt && c->dim == dim)
        {
            // remove outside quads from schedule
            if (world->take(c))
            {
                c->stopped = true;
            }
            if (gx >= 0 && gx < w && gz >= 0 && gz < h)
            {
                Quad *& g = grid[gz*w + gx];
                if (g == NULL)
                {
                    g = c;
                    continue;
                }
            }
        }
        newcache.push_back(c);
    }
    cache.swap(newcache);

    // collect which quads need generation and add any that are missing
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            Quad *& g = grid[j*w + i];
            if (g == NULL)
            {
                g = new Quad(this, x+i, z+j);
                g->prio = sqdist(i-w/2, j-h/2);
                togen.push_back(g);
            }
            else if ((g->stopped || world->take(g)) && !g->done)
            {
                g->stopped = false;
                g->prio = sqdist(i-w/2, j-h/2);
                if (g->dim != dim)
                    g->prio += 1000000;
                togen.push_back(g);
            }
        }
    }

    if (!togen.empty())
    {   // start the quad processing
        std::sort(togen.begin(), togen.end(),
                  [](Quad* a, Quad* b) { return a->prio < b->prio; });
        for (Quad *q : togen)
            world->add(q);
        world->startWorkers();
    }

    cells.swap(grid);
    tx = x;
    tz = z;
    tw = w;
    th = h;

    world->mutex.unlock();
}

void Level::update(std::vector<Quad*>& cache, qreal bx0, qreal bz0, qreal bx1, qreal bz1)
{
    int nti0 = (int) std::floor(bx0 / blocks);
    int ntj0 = (int) std::floor(bz0 / blocks);
    int nti1 = (int) std::floor(bx1 / blocks) + 1;
    int ntj1 = (int) std::floor(bz1 / blocks) + 1;

    // resize if the new area is much smaller or in an unprocessed range
    if ((nti1-nti0)*2 < tw || nti0 < tx || nti1 > tx+tw || ntj0 < tz || ntj1 > tz+th)
    {
        qreal padf = 0.2 * (bx1 - bx0);
        nti0 = (int) std::floor((bx0-padf) / blocks);
        ntj0 = (int) std::floor((bz0-padf) / blocks);
        nti1 = (int) std::floor((bx1+padf) / blocks) + 1;
        ntj1 = (int) std::floor((bz1+padf) / blocks) + 1;

        resizeLevel(cache, nti0, ntj0, nti1-nti0, ntj1-ntj0);
    }
}

void Level::setInactive(std::vector<Quad*>& cache)
{
    if (th > 0 || tw > 0)
        resizeLevel(cache, 0, 0, 0, 0);
}

void MapWorker::run()
{
    while (Scheduled *q = world->requestQuad())
    {
        q->run();
        if (q->done)
            emit quadDone();
        if (q->autoDelete())
            delete q; // manual call to run() so delete manually as well
    }
}

QWorld::QWorld(WorldInfo wi, int dim, LayerOpt lopt)
    : QObject()
    , wi(wi)
    , dim(dim)
    , mconfig()
    , lopt(lopt)
    , lvb()
    , lvs()
    , activelv()
    , cachedbiomes()
    , cachedstruct()
    , memlimit()
    , mutex()
    , queue()
    , workers(QThread::idealThreadCount())
    , threadlimit()
    , showBB()
    , gridspacing()
    , gridmultiplier()
    , spawn()
    , strongholds()
    , qsinfo()
    , isdel()
    , slimeimg()
    , slimex()
    , slimez()
    , seldo()
    , selx()
    , selz()
    , selopt(-1)
    , selvp(Pos{0,0}, -1)
    , qual()
{
    setupGenerator(&g, wi.mc,  wi.large);

    memlimit = 256ULL * 1024*1024;
    for (MapWorker& w : workers)
    {
        w.world = this;
        QObject::connect(&w, &MapWorker::quadDone, this, &QWorld::update);
    }

    setDim(dim, lopt);

    QSettings settings(APP_STRING, APP_STRING);
    mconfig.load(settings);

    lvs.resize(D_SPAWN);
    for (int opt = D_DESERT; opt < D_SPAWN; opt++)
        if (mconfig.enabled(opt))
            lvs[opt].init4struct(this, opt);
    memset(sshow, 0, sizeof(sshow));
}

QWorld::~QWorld()
{
    clear();
    for (Quad *q : cachedbiomes)
        delete q;
    for (Quad *q : cachedstruct)
        delete q;
    if (spawn && spawn != (Pos*)-1)
    {
        delete spawn;
        delete strongholds;
        delete qsinfo;
    }
}

void QWorld::setDim(int dim, LayerOpt lopt)
{
    clear();
    if (this->lopt.activeDifference(lopt))
        cleancache(cachedbiomes, 0);

    this->dim = dim;
    this->lopt = lopt;
    applySeed(&g, dim, wi.seed);
    initSurfaceNoise(&sn, dim, g.seed);

    int pixs, lcnt;
    if (lopt.mode == LOPT_HEIGHT)
    {
        pixs = 32;
        lcnt = 6;
        qual = 4.0;
    }
    else if (g.mc > MC_1_17 || dim != DIM_OVERWORLD)
    {
        pixs = 128;
        lcnt = 6;
        qual = 2.0;
    }
    else
    {
        pixs = 512;
        lcnt = 5;
        qual = 1.0;
    }

    activelv = -1;
    lvb.clear();
    lvb.resize(lcnt);
    for (int i = 0, scale = 1; i < lcnt; i++, scale *= 4)
        lvb[i].init4map(this, pixs, scale);
}

void QWorld::setSelectPos(QPoint pos)
{
    selx = pos.x();
    selz = pos.y();
    seldo = true;
    selopt = D_NONE;
}

int QWorld::getBiome(Pos p)
{
    Generator *g = &this->g;
    int scale = 1;
    int y = wi.y;

    if (activelv >= 0 && activelv < (int)lvb.size())
    {
        g = &lvb[activelv].g;
        scale = lvb[activelv].blocks / lvb[activelv].pixs;
        p.x = p.x / scale - (p.x < 0);
        p.z = p.z / scale - (p.z < 0);
        if (scale != 1)
            y /= 4;
    }

    int id = getBiomeAt(g, scale, p.x, y, p.z);
    return id;
}

QString QWorld::getBiomeName(Pos p)
{
    if (dim == 0 && lopt.mode == LOPT_STRUCTS)
        return "";
    int id = getBiome(p);
    if (dim == 0 && lopt.isClimate(wi.mc))
    {
        QString c;
        switch (lopt.mode)
        {
            case LOPT_NOISE_T_4: c = "T"; break;
            case LOPT_NOISE_H_4: c = "H"; break;
            case LOPT_NOISE_C_4: c = "C"; break;
            case LOPT_NOISE_E_4: c = "E"; break;
            case LOPT_NOISE_D_4: c = "D"; break;
            case LOPT_NOISE_W_4: c = "W"; break;
            case LOPT_BETA_T_1: c = "T"; break;
            case LOPT_BETA_H_1: c = "H"; break;
        }
        if (int disp = lopt.activeDisp())
        {
            if (wi.mc >= MC_1_18)
                c += QString::asprintf(".%d%c(%d)", (disp-1)/2, (disp-1)%2?'B':'A', disp);
            else
                c += "." + QString::number(disp);
        }
        return c + "=" + QString::number(id);
    }
    QString ret = getBiomeDisplay(wi.mc, id);
    if (lopt.mode == LOPT_HEIGHT)
    {
        int y = estimateSurface(p);
        if (y > 0)
            ret = QString::asprintf("Y~%d ", y) + ret;
    }
    return ret;
}

int QWorld::estimateSurface(Pos p)
{
    float y = 0;
    if (g.dim == DIM_END)
    {   // use end surface generator for 1:1 scale
        mapEndSurfaceHeight(&y, &g.en, &sn, p.x, p.z, 1, 1, 1, 0);
        mapEndIslandHeight(&y, &g.en, g.seed, p.x, p.z, 1, 1, 1);
    }
    else
    {
        mapApproxHeight(&y, 0, &g, &sn, p.x>>2, p.z>>2, 1, 1);
    }
    return (int) floor(y);
}

void QWorld::refreshBiomeColors()
{
    QMutexLocker locker(&g_mutex);
    for (Level& l : lvb)
    {
        for (Quad *q : l.cells)
            q->updateBiomeColor();
    }
    for (Quad *q : cachedbiomes)
        q->updateBiomeColor();
}

void QWorld::clear()
{
    mutex.lock();
    queue = nullptr;
    isdel = true;
    mutex.unlock();
    waitForIdle();
    isdel = false;

    for (Level& l : lvb)
        l.setInactive(cachedbiomes);
    for (Quad *q : cachedbiomes)
    {
        if (!q->done)
            q->stopped = true;
    }
}

void QWorld::startWorkers()
{
    int n = (int) workers.size();
    if (threadlimit && threadlimit < n)
        n = threadlimit;
    for (int i = 0; i < n; i++)
        workers[i].start();
}

void QWorld::waitForIdle()
{
    for (MapWorker& w : workers)
        w.wait(-1);
}

bool QWorld::isBusy()
{
    for (MapWorker& w : workers)
        if (w.isRunning())
            return true;
    return false;
}

void QWorld::add(Scheduled *q)
{
    //QMutexLocker locker(&mutex);
    if (!queue)
    {   // new start
        q->next = nullptr;
        queue = q;
        return;
    }
    if (q->prio <= queue->prio)
    {   // new head
        q->next = queue;
        queue = q;
        return;
    }
    Scheduled *prev = queue, *p = prev->next;
    while (p)
    {
        if (p->prio >= q->prio)
            break;
        prev = p;
        p = p->next;
    }
    q->next = p;
    prev->next = q;
}

Scheduled *QWorld::take(Scheduled *q)
{
    //QMutexLocker locker(&mutex);
    if (Scheduled *p = queue)
    {
        if (p == q)
        {
            queue = p->next;
            return p;
        }
        Scheduled *prev = p;
        while ((p = p->next))
        {
            if (p == q)
            {
                prev->next = p->next;
                return p;
            }
            prev = p;
        }
    }
    return nullptr;
}

Scheduled *QWorld::requestQuad()
{
    if (isdel)
        return nullptr;
    QMutexLocker locker(&mutex);
    Scheduled *q;
    if ((q = queue))
        queue = q->next;
    return q;
}

void QWorld::cleancache(std::vector<Quad*>& cache, unsigned int maxsize)
{
    size_t n = cache.size();
    if (n < maxsize)
        return;

    size_t targetsize = 4 * maxsize / 5;

    mutex.lock();
    std::vector<Quad*> newcache;
    for (size_t i = 0; i < n; i++)
    {
        Quad *q = cache[i];
        if (q->stopped)
        {
            delete q;
            continue;
        }
        if (n - i >= targetsize)
        {
            if (q->done || take(q))
            {
                delete q;
                continue;
            }
        }
        newcache.push_back(q);
    }
    //printf("%zu -> %zu [%u]\n", n, newcache.size(), maxsize);
    cache.swap(newcache);
    mutex.unlock();
}

struct SpawnStronghold : public Scheduled
{
    QWorld *world;
    WorldInfo wi;

    SpawnStronghold(QWorld *world, WorldInfo wi)
        : Scheduled(), world(world),wi(wi)
    {
        setAutoDelete(true);
    }

    void run()
    {
        Generator g;
        setupGenerator(&g, wi.mc, wi.large);
        applySeed(&g, 0, wi.seed);

        Pos *p = new Pos;
        *p = getSpawn(&g);
        world->spawn = p;
        if (world->isdel) return;

        StrongholdIter sh;
        initFirstStronghold(&sh, wi.mc, wi.seed);

        // note: pointer to atomic pointer
        QAtomicPointer<PosElement> *shpp = &world->strongholds;

        while (nextStronghold(&sh, &g) > 0)
        {
            if (world->isdel)
                return;
            PosElement *shp;
            (*shpp) = shp = new PosElement(sh.pos);
            shpp = &shp->next;
        }

        QVector<QuadInfo> *qsinfo = new QVector<QuadInfo>;

        if (!world->isdel)
            findQuadStructs(Swamp_Hut, &g, qsinfo);
        if (!world->isdel)
            findQuadStructs(Monument, &g, qsinfo);

        world->qsinfo = qsinfo;
    }
};

static bool draw_grid_rec(QPainter& painter, QColor col, QRect &rec, qreal pix, int64_t x, int64_t z)
{
    painter.setPen(QPen(col, 1));
    painter.drawRect(rec);
    if (pix < 50)
        return false;

    QString s = QString::asprintf("%" PRId64 ",%" PRId64, x, z);
    QRect textrec = painter.fontMetrics()
            .boundingRect(rec, Qt::AlignLeft | Qt::AlignTop, s);
    if (textrec.width() > pix)
    {
        s.replace(",", "\n");
        textrec = painter.fontMetrics()
                .boundingRect(rec, Qt::AlignLeft | Qt::AlignTop, s);
        if (textrec.width() > pix)
            return false;
    }
    painter.fillRect(textrec, QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));

    painter.setPen(QColor(255, 255, 255));
    painter.drawText(textrec, s);
    return true;
}

void QWorld::draw(QPainter& painter, int vw, int vh, qreal focusx, qreal focusz, qreal blocks2pix)
{
    qreal uiw = vw / blocks2pix;
    qreal uih = vh / blocks2pix;

    qreal bx0 = focusx - uiw/2;
    qreal bz0 = focusz - uih/2;
    qreal bx1 = focusx + uiw/2;
    qreal bz1 = focusz + uih/2;

    // determine the active level, which represents a scale resolution of:
    // [0] 1:1, [1] 1:4, [2] 1:16, [3] 1:64, [4] 1:256
    qreal imgres = qual;
    for (activelv = 0; activelv < (int)lvb.size(); activelv++, imgres /= 4)
        if (blocks2pix > imgres)
            break;
    activelv--;

    QFont oldfont = painter.font();
    QFont smallfont = oldfont;
    smallfont.setPointSize(oldfont.pointSize() - 2);
    painter.setFont(smallfont);

    QColor gridcol = lopt.mode == LOPT_HEIGHT ? QColor(192, 0, 0, 96) : QColor(0, 0, 0, 96);
    int gridpix = 128;
    // 128px is approximately the size of:
    //gridpix = painter.fontMetrics().boundingRect("-30000000,-30000000").width();
    // and sets the scale changes such that they align with the power of 4 tiles

    for (int li = activelv+1; li >= activelv; --li)
    {
        if (li < 0 || li >= (int)lvb.size())
            continue;
        Level& l = lvb[li];
        for (Quad *q : l.cells)
        {
            if (!q->img)
                continue;
            // q was processed in another thread and is now done
            qreal ps = q->blocks * blocks2pix;
            qreal px = vw/2.0 + (q->ti) * ps - focusx * blocks2pix;
            qreal pz = vh/2.0 + (q->tj) * ps - focusz * blocks2pix;
            QRect rec(floor(px),floor(pz), ceil(ps),ceil(ps));
            //QImage img = (*q->img).scaledToWidth(rec.width(), Qt::SmoothTransformation);
            painter.drawImage(rec, *q->img);

            if (sshow[D_GRID] && !gridspacing)
            {
                draw_grid_rec(painter, gridcol, rec, ps, q->ti*q->blocks, q->tj*q->blocks);
            }
        }
    }

    if (sshow[D_GRID] && gridspacing)
    {
        int64_t gs = gridspacing;

        while (true)
        {
            long x = floor(bx0 / gs), w = floor(bx1 / gs) - x + 1;
            long z = floor(bz0 / gs), h = floor(bz1 / gs) - z + 1;
            qreal ps = gs * blocks2pix;

            if (ps > gridpix)
            {
                for (int j = 0; j < h; j++)
                {
                    for (int i = 0; i < w; i++)
                    {
                        qreal px = vw/2.0 + (x+i) * ps - focusx * blocks2pix;
                        qreal pz = vh/2.0 + (z+j) * ps - focusz * blocks2pix;
                        QRect rec(px, pz, ps, ps);
                        draw_grid_rec(painter, gridcol, rec, ps, (x+i)*gs, (z+j)*gs);
                    }
                }
                break;
            }
            if (gridmultiplier <= 1)
                break;
            gs *= gridmultiplier;
        }
    }

    painter.setFont(oldfont);

    if (sshow[D_SLIME] && dim == 0 && blocks2pix*16 > 0.5)
    {
        long x = floor(bx0 / 16), w = floor(bx1 / 16) - x + 1;
        long z = floor(bz0 / 16), h = floor(bz1 / 16) - z + 1;

        // conditions when the slime overlay should be updated
        if (x < slimex || z < slimez ||
            x+w >= slimex+slimeimg.width() || z+h >= slimez+slimeimg.height() ||
            w*h*4 >= slimeimg.width()*slimeimg.height())
        {
            int pad = (int)(20 / blocks2pix); // 20*16 pixels movement before recalc
            x -= pad;
            z -= pad;
            w += 2*pad;
            h += 2*pad;
            slimeimg = QImage(w, h, QImage::Format_Indexed8);
            slimeimg.setColor(0, qRgba(0, 0, 0, 64));
            slimeimg.setColor(1, qRgba(0, 255, 0, 64));
            slimex = x;
            slimez = z;

            for (int j = 0; j < h; j++)
            {
                for (int i = 0; i < w; i++)
                {
                    int isslime = isSlimeChunk(wi.seed, i+x, j+z);
                    slimeimg.setPixel(i, j, isslime);
                }
            }
        }

        qreal ps = 16 * blocks2pix;
        qreal px = vw/2.0 + slimex * ps - focusx * blocks2pix;
        qreal pz = vh/2.0 + slimez * ps - focusz * blocks2pix;

        QRect rec(round(px), round(pz), round(ps*slimeimg.width()), round(ps*slimeimg.height()));
        painter.drawImage(rec, slimeimg);
    }

    // draw bounding boxes and shapes
    painter.setPen(QPen(QColor(192, 0, 0, 160), 1));

    if (showBB && blocks2pix >= 1.0 && qsinfo && dim == 0)
    {
        for (QuadInfo qi : qAsConst(*qsinfo))
        {
            if (qi.typ == Swamp_Hut && !sshow[D_HUT])
                continue;
            if (qi.typ == Monument && !sshow[D_MONUMENT])
                continue;

            qreal x = vw/2.0 + (qi.afk.x - focusx) * blocks2pix;
            qreal y = vh/2.0 + (qi.afk.z - focusz) * blocks2pix;
            qreal r = 128.0 * blocks2pix;
            painter.drawEllipse(QRectF(x-r, y-r, 2*r, 2*r));
            r = 16;
            painter.drawLine(QPointF(x-r,y), QPointF(x+r,y));
            painter.drawLine(QPointF(x,y-r), QPointF(x,y+r));
        }
    }

    if (showBB && sshow[D_GATEWAY] && dim == DIM_END && g.mc > MC_1_12)
    {
        if (endgates.empty())
        {
            endgates.resize(40);
            getFixedEndGateways(g.mc, g.seed, &endgates[0]);
            for (int i = 0; i < 20; i++)
               endgates[20 + i] = getLinkedGatewayPos(&g.en, &sn, g.seed, endgates[i]);
        }

        for (int i = 0; i < 20; i++)
        {
            qreal xsrc = vw/2.0 + (0.5 + endgates[i].x - focusx) * blocks2pix;
            qreal ysrc = vh/2.0 + (0.5 + endgates[i].z - focusz) * blocks2pix;
            qreal xdst = vw/2.0 + (0.5 + endgates[i+20].x - focusx) * blocks2pix;
            qreal ydst = vh/2.0 + (0.5 + endgates[i+20].z - focusz) * blocks2pix;

            QPen pen = painter.pen();
            painter.setPen(QPen(QColor(192, 0, 0, 160), i == 0 ? 1.5 : 0.5));
            painter.drawLine(QPointF(xsrc,ysrc), QPointF(xdst,ydst));
            painter.setPen(pen);

            if (blocks2pix >= 1.0 && abs(xsrc) < vw && abs(ysrc) < vh)
            {
                QString s = QString::number(i+1);
                QRect rec = painter.fontMetrics().boundingRect(s);
                qreal dx = xsrc - xdst;
                qreal dy = ysrc - ydst;
                qreal df = rec.height() / sqrt(dx*dx + dy*dy);
                rec.moveCenter(QPoint(xsrc + dx * df, ysrc + dy * df));
                painter.drawText(rec, s);
            }
        }
    }

    for (Shape& s : shapes)
    {
        if (s.dim != DIM_UNDEF && s.dim != dim)
            continue;
        qreal x1 = vw/2.0 + (s.p1.x - focusx) * blocks2pix;
        qreal y1 = vh/2.0 + (s.p1.z - focusz) * blocks2pix;
        qreal x2 = vw/2.0 + (s.p2.x - focusx) * blocks2pix;
        qreal y2 = vh/2.0 + (s.p2.z - focusz) * blocks2pix;
        qreal r = s.r * blocks2pix;
        switch (s.type)
        {
        case Shape::RECT:
            painter.drawRect(QRectF(x1, y1, x2-x1, y2-y1));
            break;
        case Shape::LINE:
            painter.drawLine(QLineF(x1, y1, x2, y2));
            break;
        case Shape::CIRCLE:
            painter.drawEllipse(QPointF(x1, y1), r, r);
            break;
        }
    }

    if (imgbuf.width() != vw || imgbuf.height() != vh)
    {
        imgbuf = QImage(vw, vh, QImage::Format_Indexed8);
        imgbuf.setColor(0, qRgba(0, 0, 0, 0));
        imgbuf.setColor(1, qRgba(255, 255, 255, 96));
        imgbuf.setColor(2, qRgba(180, 64, 192, 255));
    }

    for (int sopt = D_DESERT; sopt < D_SPAWN; sopt++)
    {
        Level& l = lvs[sopt];
        if (!sshow[sopt] || dim != l.dim || l.vis > blocks2pix)
            continue;
        if (lopt.mode == LOPT_STRUCTS)
        {
            if (sopt == D_GEODE || sopt == D_WELL)
                continue;
        }

        std::map<const QPixmap*, std::vector<QPainter::PixmapFragment>> frags;
        bool imgbuf_used = false;
        imgbuf.fill(0);

        for (Quad *q : l.cells)
        {
            if (!q->spos)
                continue;
            // q was processed in another thread and is now done
            for (VarPos& vp : *q->spos)
            {
                qreal x = vw/2.0 + (vp.p.x - focusx) * blocks2pix;
                qreal y = vh/2.0 + (vp.p.z - focusz) * blocks2pix;

                const QPixmap& icon = getMapIcon(sopt, &vp);
                QRectF rec = icon.rect();

                if (x < -rec.width()  || x >= vw+rec.width() ||
                    y < -rec.height() || y >= vh+rec.width())
                {
                    continue;
                }

                if (showBB && blocks2pix > 0.5)
                {
                    if (vp.v.sx && vp.v.sz)
                    {   // draw bounding box and move icon to its center
                        x += vp.v.x * blocks2pix;
                        y += vp.v.z * blocks2pix;
                        qreal dx = vp.v.sx * blocks2pix;
                        qreal dy = vp.v.sz * blocks2pix;
                        painter.setPen(QPen(QColor(192, 0, 0, 160), 1));
                        painter.drawRect(QRect(x, y, dx, dy));
                        // center icon on bb
                        x += dx / 2;
                        y += dy / 2;
                    }
                    else if (vp.type == Geode)
                    {
                        qreal rad = vp.v.size * blocks2pix;
                        x += vp.v.x * blocks2pix;
                        y += vp.v.z * blocks2pix;
                        painter.setPen(QPen(QColor(192, 0, 0, 160), 1));
                        painter.drawEllipse(QPointF(x, y), rad, rad);
                    }
                    painter.setPen(QPen(QColor(192, 0, 0, 128), 0));
                    for (Piece& p : vp.pieces)
                    {
                        qreal px = vw/2.0 + (p.bb0.x - focusx) * blocks2pix;
                        qreal py = vh/2.0 + (p.bb0.z - focusz) * blocks2pix;
                        qreal dx = (p.bb1.x - p.bb0.x + 1) * blocks2pix;
                        qreal dy = (p.bb1.z - p.bb0.z + 1) * blocks2pix;
                        painter.drawRect(QRect(px, py, dx, dy));

                        if (vp.type == Fortress && p.type == BRIDGE_SPAWNER)
                        {
                            QRectF spawner = QRectF(px, py, blocks2pix, blocks2pix);
                            spawner.translate((dx - blocks2pix) / 2, (dy - blocks2pix) / 2);
                            static const Pos sp_off[4] = { {0,-1},{+1,0},{0,+1},{-1,0} };
                            qreal offx = sp_off[p.rot].x - (p.bb0.x < 0);
                            qreal offz = sp_off[p.rot].z - (p.bb0.z < 0);
                            spawner.translate(blocks2pix * offx, blocks2pix * offz);
                            painter.drawRect(spawner);
                        }
                    }
                }

                QPointF d = QPointF(x, y);

                Pos spos = {
                    vp.p.x ,//+ (vp.v.x + vp.v.sx / 2),
                    vp.p.z //+ (vp.v.z + vp.v.sz / 2)
                };

                if (sopt == D_GEODE)
                {
                    int r = 2;
                    QRectF rec = QRectF(d.x()-r, d.y()-r, 2*r, 2*r);
                    if (seldo && rec.contains(selx, selz))
                    {
                        selopt = sopt;
                        selvp = vp;
                    }
                    if (selopt == sopt && selvp.p.x == spos.x && selvp.p.z == spos.z)
                    {   // don't draw selected structure
                        continue;
                    }
                    for (int i = -r; i <= r; i++)
                    {
                        for (int j = -r; j <= r; j++)
                        {
                            int q = i*i + j*j;
                            if (q > r*r) continue;
                            int x = floor(d.x() + i);
                            int z = floor(d.y() + j);
                            if (imgbuf.rect().contains(x, z))
                            {
                                imgbuf.setPixel(x, z, q >= 2 ? 1 : 2);
                                imgbuf_used = true;
                            }
                        }
                    }
                    continue;
                }

                if (seldo)
                {   // check for structure selection
                    QRectF r = rec;
                    r.moveCenter(d);
                    if (r.contains(selx, selz))
                    {
                        selopt = sopt;
                        selvp = vp;
                    }
                }
                if (selopt == sopt && selvp.p.x == spos.x && selvp.p.z == spos.z)
                {   // don't draw selected structure
                    continue;
                }

                frags[&icon].push_back(QPainter::PixmapFragment::create(d, rec));
            }
        }

        if (imgbuf_used)
            painter.drawImage(QPoint(0, 0), imgbuf);
        for (auto& it : frags)
            painter.drawPixmapFragments(it.second.data(), it.second.size(), *it.first);
    }

    Pos* sp = spawn; // atomic fetch
    if (sp && sp != (Pos*)-1 && sshow[D_SPAWN] && dim == 0 && lopt.mode != LOPT_STRUCTS)
    {
        qreal x = vw/2.0 + (sp->x - focusx) * blocks2pix;
        qreal y = vh/2.0 + (sp->z - focusz) * blocks2pix;

        QPointF d = QPointF(x, y);
        QRectF r = getMapIcon(D_SPAWN).rect();
        painter.drawPixmap(x-r.width()/2, y-r.height()/2, getMapIcon(D_SPAWN));

        if (seldo)
        {
            r.moveCenter(d);
            if (r.contains(selx, selz))
            {
                selopt = D_SPAWN;
                selvp = VarPos(*sp, -1);
            }
        }
    }

    QAtomicPointer<PosElement> shs = strongholds;
    if (shs && sshow[D_STRONGHOLD] && dim == 0 && lopt.mode != LOPT_STRUCTS)
    {
        std::vector<QPainter::PixmapFragment> frags;
        do
        {
            Pos p = (*shs).p;
            qreal x = vw/2.0 + (p.x - focusx) * blocks2pix;
            qreal y = vh/2.0 + (p.z - focusz) * blocks2pix;
            QPointF d = QPointF(x, y);
            QRectF r = getMapIcon(D_STRONGHOLD).rect();
            frags.push_back(QPainter::PixmapFragment::create(d, r));

            if (seldo)
            {
                r.moveCenter(d);
                if (r.contains(selx, selz))
                {
                    selopt = D_STRONGHOLD;
                    selvp = VarPos(p, -1);
                }
            }
        }
        while ((shs = (*shs).next));
        painter.drawPixmapFragments(frags.data(), frags.size(), getMapIcon(D_STRONGHOLD));
    }

    for (int sopt = D_DESERT; sopt < D_SPAWN; sopt++)
    {
        Level& l = lvs[sopt];
        if (l.vis <= blocks2pix && sshow[sopt] && dim == l.dim)
            l.update(cachedstruct, bx0, bz0, bx1, bz1);
        else if (l.vis * 4 > blocks2pix)
            l.setInactive(cachedstruct);
    }
    for (int li = lvb.size()-1; li >= 0; --li)
    {
        Level& l = lvb[li];
        if (li == activelv || li == activelv+1)
            l.update(cachedbiomes, bx0, bz0, bx1, bz1);
        else
            l.setInactive(cachedbiomes);
    }

    if (spawn == NULL && lopt.mode != LOPT_STRUCTS)
    {   // start the spawn and stronghold worker thread if this is the first run
        if (sshow[D_SPAWN] || sshow[D_STRONGHOLD] || (showBB && blocks2pix >= 1.0))
        {
            spawn = (Pos*) -1;
            mutex.lock();
            SpawnStronghold *work = new SpawnStronghold(this, wi);
            add(work);
            startWorkers();
            mutex.unlock();
        }
    }

    if (seldo)
    {
        seldo = false;
    }

    if (selopt != D_NONE)
    {   // draw selection overlay
        const QPixmap& icon = getMapIcon(selopt, &selvp);
        qreal x = vw/2.0 + (selvp.p.x - focusx) * blocks2pix;
        qreal y = vh/2.0 + (selvp.p.z - focusz) * blocks2pix;
        QRect iconrec = icon.rect();
        qreal w = iconrec.width() * 1.49;
        qreal h = iconrec.height() * 1.49;
        painter.drawPixmap(x-w/2, y-h/2, w, h, icon);

        QFont f = QFont();
        f.setBold(true);
        painter.setFont(f);

        QString s = QString::asprintf(" %d,%d", selvp.p.x, selvp.p.z);
        int pad = 5;
        QRect textrec = painter.fontMetrics()
                .boundingRect(0, 0, vw, vh, Qt::AlignLeft | Qt::AlignTop, s);

        if (textrec.height() < iconrec.height())
            textrec.setHeight(iconrec.height());

        textrec.translate(pad+iconrec.width(), pad);

        painter.fillRect(textrec.marginsAdded(QMargins(pad+iconrec.width(),pad,pad,pad)),
                         QBrush(QColor(0, 0, 0, 128), Qt::SolidPattern));

        painter.setPen(QPen(QColor(255, 255, 255), 2));
        painter.drawText(textrec, s, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));

        iconrec = iconrec.translated(pad,pad);
        painter.drawPixmap(iconrec, icon);

        QStringList sinfo = selvp.detail();
        if (!sinfo.empty())
        {
            f = QFont();
            f.setPointSize(8);
            painter.setFont(f);

            s = sinfo.join(":");
            int xpos = textrec.right() + 3;
            textrec = painter.fontMetrics().boundingRect(0, 0, vw, vh, Qt::AlignLeft | Qt::AlignTop, s);
            textrec = textrec.translated(xpos+pad, 0);
            painter.fillRect(textrec.marginsAdded(QMargins(2,0,2,0)), QBrush(QColor(0,0,0,128), Qt::SolidPattern));
            painter.drawText(textrec, s, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
        }
    }

    if (activelv < 0)
        activelv = 0;
    if (activelv >= (int)lvb.size()-1)
        activelv = lvb.size() - 1;
    int pixs = lvb[activelv].pixs;
    unsigned int cachesize = memlimit / pixs / pixs / (3+4); // sizeof(RGB) + sizeof(biome_id)

    cleancache(cachedbiomes, cachesize);
    cleancache(cachedstruct, cachesize);

    if (0)
    {   // debug outline
        qreal rx0 = vw/2.0 + (bx0 - focusx) * blocks2pix;
        qreal rz0 = vh/2.0 + (bz0 - focusz) * blocks2pix;
        qreal rx1 = vw/2.0 + (bx1 - focusx) * blocks2pix;
        qreal rz1 = vh/2.0 + (bz1 - focusz) * blocks2pix;
        painter.setPen(QPen(QColor(255, 0, 0, 255), 1));
        painter.drawRect(QRect(rx0, rz0, rx1-rx0, rz1-rz0));
    }
}


