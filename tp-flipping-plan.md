# GW2 Trading Post Flipping — Araştırma & Addon Planı

## Bölüm 1: Araştırma Bulguları (Eylül 2026)

### TP Vergi Yapısı
- Listeleme ücreti: %5 (peşin, iade yok)
- Satış komisyonu: %10
- **Toplam vergi: %15**
- **Kâra geçiş eşiği: min. %17.65 spread**
- Formül: `Kâr = (Satış Fiyatı × 0.85) − Alış Fiyatı`

### Sıralama Metriği
Tablolar **günlük satış hacmi × birim kâr** üzerinden sıralanmıştır.
ROI tek başına yanıltıcıdır — %106 ROI'li 7c'lik bir item, günde sadece 445 adet
satılıyorsa toplam 31s/gün potansiyel taşır. Asıl önemli olan sirkülasyon hızıdır.

> **DÜZELTME (Eylül 2026, oyun içi test sonrası):** Yukarıdaki metrik *pazarın* büyüklüğünü
> ölçer, *tek bir oyuncunun* kazancını değil. Aşağıdaki "Emir Ekonomisi" bölümü bunu düzeltir.
> Copper-tier tablo, ≥50g sermayeli oyuncu için **tuzak** — geçersiz sayılmalı.

### Emir Ekonomisi — Tek Oyuncu İçin Doğru Metrik

**İki sert kısıt:**
1. **Emir başı sabit efor.** TP'de bir emir max 250 birim. Her 250'lik döngü = alış emri ver →
   bekle → topla → satış listele → bekle → altını topla. Bu efor 15c'lik item için de 50s'lik
   için de aynıdır (~2 dk aktif ilgi + takip). 200 açık pozisyon izlenemez; 10-20 izlenir.
2. **Pazar emilimi.** Günlük hacmin ancak ~%10'unu çevirebilirsin; fazlası fiyatı bozar.

**Örnek — Congealed Putrescence (15c → 26c, 8c kâr):**
- 1 stack (250) = 37s sermaye, **20s kâr**. 100g yatırmak için **266 emir** = 66.500 birim =
  pazarın **4 günlük tüm hacmi**. Fiziksel olarak imkansız.
- Gerçekçi %10 pay = ~1.700 birim/gün = 7 emir/gün çevirip **1.4g/gün**. Efora değmez.

**Doğru formül (oyuncu bazlı):**
```
emir_boyutu  = min(250, sermaye_pozisyon / alış_fiyatı)        sermaye_pozisyon = 20g (100g'ın %20'si)
kâr/emir     = birim_kâr × emir_boyutu
akış/gün     = min(Sold/gün, Bought/gün) × pay                  pay ≈ %10 (VARSAYIM — pazarı bozmama sınırı)
emir/gün     = akış/gün / emir_boyutu
gün/kâr      = birim_kâr × akış/gün
```
**Sold ≠ Bought.** GW2BLTC'de *Sold* = satış listelerinden alıcıya giden birim; *Bought* = alış
emirlerine dökülen birim. Flipper'ın alış emri **Bought**'tan dolar. Bought düşükse spread sahtedir:
kimse alış emri açmaz çünkü dolmaz → talep derinliği çöker → spread açık kalır. Resmi API'de ikisi de yok.

**Eşik: kâr/emir ≥ 3g.** Gerekçe (kullanıcının kendi argümanı):
- Emir başı efor sabit; 100g / (%30-50 ROI, 3g/emir) → 10-16 eşzamanlı pozisyon = takip edilebilir. 200 değil.
- Pratikte: **birim kâr ≥ ~1.2s** (tam stack'te 3g) ya da pahalı itemde 20g/alış kadar adet.
- (Farming karşılaştırması ~15-25g/saat bir VARSAYIM; eşik ona dayanmıyor.)

**Sistematik tarama (GW2BLTC, 3 fiyat bandı + Bought geçişi, 5 sayfa; kâr/emir ≥3g @20g, Sold ≥200 ∧ Bought ≥200,
mevsimsel/troll hariç; ID'ler API ile doğrulandı). Kâr/emir'e göre sıralı — bunlar addon'un varsayılan listesi:**

| Item | ID | Alış | Kâr/unit | Emir | **Kâr/Emir** | Sold/g | Bought/g | Arz/Talep | Not |
|------|----|------|----------|------|--------------|--------|----------|-----------|-----|
| Mystic Aspect | 89105 | 1g27s | 60s | 15 | **9g00s** | 551 | 601 | 8.8K/4.6K | ★★★ Gift of Runes malz., ROI %47, iki taraf likit |
| Plate of Beef Rendang | 86997 | 11s61c | 4s49c | 172 | **7g72s** | 263 | 284 | 32.6K/4.6K | ⚠ arz 7× talep → 32K'lık satış kuyruğu, undercut şart |
| Salvageable Intact Forged Scrap | 82488 | 2s22c | 3s05c | 250 | **7g62s** | 206 | 676 | 64K/32K | ⚠ 64K arz, Sold tam sınırda → satış yavaş |
| Molten Fragment | 24312 | 8s00c | 2s15c | 250 | **5g37s** | 282 | 361 | 15K/44K | ★★ (önceki "25 adet/53s" hesabı 10× hatalıydı) |
| Master Tuning Crystal | 9476 | 12s81c | 3s34c | 156 | **5g21s** | 547 | 1,222 | 30K/12.6K | ★★ Artificer 400, 1 Dust → tavan ≈ dust/0.85 |
| Potent Master Tuning Crystal | 43449 | 16s07c | 3s48c | 124 | 4g31s | 281 | 539 | 22K/6.2K | Artificer 400 |
| Powerful Potion of Dredge Slaying | 8892 | 1s35c | 1s71c | 250 | 4g27s | 740 | 639 | 32K/17.5K | Artificer 400 |
| Shard of Mistburned Barrens | 104282 | 1g36s | 29s62c | 14 | 4g14s | 386 | 355 | 3.4K/2.4K | JW legendary; vendor 20/hafta → tavan |
| Raspberry Passion Fruit Compote | 36782 | 1s23c | 1s46c | 250 | 3g65s | 2,172 | 471 | 6.8K/55K | ★★ Chef 350, satış çok hızlı |
| +7 Agony Infusion | 49430 | 1g00s | 19s09c | 19 | 3g62s | 230 | 228 | 3.4K/3.7K | INFUZ 4×+5 → fiyat +5'e bağlı |
| Blackberry Cookie | 12383 | 2s00c | 1s41c | 250 | 3g52s | 218 | 262 | 7.1K/35K | Chef 225; Bought tek kaynaklı |
| Powerful Potion of Demon Slaying | 8886 | 5s74c | 1s34c | 250 | 3g35s | 1,527 | 970 | 57K/17K | Artificer 400, Kryptis talebi |
| Badge of Tribute | 71473 | 15s95c | 2s57c | 125 | 3g21s | 1,772 | 1,714 | 21K/35K | ★★ en hızlı döngü; ROI %16 ince, vendor tavanı |
| Iron Plated Dowel | 12993 | 2s08c | 1s26c | 250 | 3g15s | 771 | 1,231 | 22K/20K | refinement 125 |
| Mystic Curio | 79410 | 39s28c | 6s28c | 50 | 3g14s | 727 | 1,066 | 39K/25K | HoT legendary malz. |
| Bottle of Simple Dressing | 12176 | 1s17c | 1s22c | 250 | 3g05s | 320 | 1,020 | 51K/31K | Chef 25 |
| Bronze Plated Dowel | 12990 | 64c | 1s06c | 250 | 2g65s | 660 | 762 | 15K/34K | sınırda — filtre gizler, listede kalsın |

**Sınırda kalanlar (tek kural bozuk):** Grilled Mushroom 5g65s (Bought 168), Bowl of Prickly Pear Sorbet 4g45s
(Bought 168), +5 Agony Infusion 4g15s (Bought 194), Ley-Line Infused Tool 3g20s (Bought 151), Superior Sigil of
Accuracy 2g70s, Fine Rift Motivation 2g58s. Bought 150-200 bandı: alış birkaç gün sürer, aksi halde uygun.

**Elenenler:** ~~Bag of Radiant Energy~~ (Bought **2**/g, %40 ROI hayali), ~~Brilliant Opal Jewel~~ (Bought **8**),
Glazed Pear Tart (Bought 17), Minor Potion of Ogre Slaying (18), Potion of Azantil (0), Sunstone/Onyx Lodestone
(kâr/emir ~1g), Unstable Rift Motivation (ROI %3) + tüm festival itemleri.

**Tarama notu:** GW2BLTC `sold-min` / `bought-min` / `sort=bought` parametreleri **çalışmıyor** (sonuçlar talebe göre
sıralı geliyor); `profit-min`, `buy-min/max`, `ipg` çalışıyor. Bought elle filtrelenmeli. Fetch'te Bought↔Bids
karışabiliyor — sütun sırasını (Sold, Offers, Bought, Bids) belirt.

**Ters köşe örneği — Ghostly Infusion (Power, 77310), Eylül 2026:** alış 76g10s, satış 92g10s.
16g'lik spread büyük *görünür*, ama 13.8g'si vergi → net kâr **2g18s, ROI %2.87**. Sold 48 / Bought 60 gün
(likit!) ama: 100g'ın %76'sı tek birimde; listeleme ücreti 4.6g peşin/iadesiz → tek undercut+relist =
net −2.4g; %3 fiyat oynaması kârı siler; Glenna vendor (1000 Magnetite + 20g) fiyata tavan koyar.
Ders: **mutlak spread ≠ kâr — ham spread %17.65'in ne kadar üstünde, ona bak.** Stat varyantlarının
%7-28 ROI'si Radiant tuzağıyla aynı: her stat ayrı item, hacim yok, spread sahte.

**Tuzak imzası (addon'da tespit edilebilir):** talep derinliği <~1000 birim ∧ arz > 3× talep → alış tarafı ölü.
Addon bunu **"ALIM RİSKLİ"** olarak işaretler (Radiant: 239 / 1,885 · Opal: 295 / 1,994 yakalanır).

**Sonuç:** İlk "2 aday" sayımı **eksikti** — 10s-80s ve 80s+ bantları taranmamıştı ve Molten emir adedi 10×
yanlıştı. Doğru taramada **16 aday** (3g05s – 9g/emir), en güçlüleri Mystic Aspect, Molten Fragment, Master Tuning
Crystal, Compote, Badge. Yine de kesişim ~28K item içinde 16 → verimli pazar tezi geçerli. Addon **kâr/emir**
hesaplayıp eşik altını gizler; aday keşfi: GW2BLTC `profit-min≥120` + fiyat bandı, **Bought ≥200 elle kontrol**.

**Elenenler (mevsimsel — festivalde arz patlar, fiyat çöker):** Strawberry Ghost, Candy Corn (Halloween),
Ugly Wool*, Peppermint Cake, Mintberry, Tuning Icicle, Cinnamon Sugar, Wintersday Gift (Wintersday),
Super Loot Bag (SAB Nisan; ayrıca ROI %10 ince).

---

### Düşük Birim Fiyat (birim <1s)
Az altınla başlanabilir. Birim fiyat düşük, hacim yüksek. Altın/gün'e göre sıralı.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Pile of Coarse Sand | 77c | 1s13c | 19c | %24.7 | 10,926 | ~20g 76s |
| 2 | Green Wood Plank | 20c | 42c | 16c | %78.5 | 12,177 | ~19g 48s |
| 3 | Powdered Rose Quartz | 47c | 65c | 8c | %17.6 | 18,979 | ~15g 18s |
| 4 | Congealed Putrescence | 13c | 23c | 7c | %50.4 | 17,443 | ~12g 21s |
| 5 | Walnut | 42c | 62c | 11c | %25.5 | 11,055 | ~12g 16s |
| 6 | Seasoned Wood Log | 45c | 64c | 9c | %20.9 | 13,188 | ~11g 87s |

### Düşük Sermaye — İnce Marj (Dikkatli ol)
Çok yüksek günlük işlem, ancak marj %15 eşiğine yakın. Küçük fiyat değişiminde zarar riski.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Hard Wood Log | 75c | 95c | 6c | %7.7 | 31,490 | ~18g 89s |
| 2 | Eye of Kormir | 26c | 37c | 5c | %21 | 23,263 | ~11g 63s |
| 3 | Glass of Buttered Spirits | 6c | 13c | 5c | %84.2 | 7,008 | ~3g 50s |

### Yüksek Birim Fiyat (birim 1s+)
Birim başına daha fazla kâr, undercut'a karşı daha dayanıklı. Altın/gün'e göre sıralı.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Super Loot Bag | 94s86c | 1g22s95c | 9s65c | %10.2 | 3,686 | ~355g pazar |
| 2 | Bag of Radiant Energy | 52s62c | 87s | 21s33c | %40.5 | 335 | ~71g 46s |
| 3 | Brilliant Opal Jewel | 1s30c | 3s98c | 2s8c | %160 | 715 | ~14g 89s |
| 4 | Corsair Tuning Crystal | 78c | 1s45c | 45c | %57.7 | 1,822 | ~8g 20s |
| 5 | Candy Corn Almond Brittle | 1s5c | 1s92c | 58c | %55.4 | 467 | ~2g 71s ⚠️ Halloween riski |

### KAÇINILACAK İtemler
- **Mystic Coin** — -%11.8 ROI (GW2 API: buy 195s90c / sell 203s19c → net -23s19c)
- **Glob of Ectoplasm** — Negatif marj
- **Strawberry Ghost** — Halloween (Ekim) öncesi fiyat çökme riski
- **Candy Corn Almond Brittle** — Aynı Halloween riski (mevsimsel item)

### Alternatif Stratejiler
1. **Time-Gated Crafting** (~1-3g/gün): Deldrimor Steel, Bolt of Damask, Elonian Leather, Spiritwood
2. **Rare Salvage → Ecto**: Level 68+ rare al → salvage → ~0.9 ecto/rare
3. **Mystic Forge T5→T6**: 50 T5 + 1 T6 + 5 Dust + 5 Philosopher's Stone = ort. [doğrulanacak] T6

### Risk Kuralları
- Tek iteme likit altının max %10-20'si
- Toplam altının %20-40'ını nakit tut
- 1c undercut 7c marjın %14'ünü siler — düşük copper itemlerde dikkat
- "Gün/Satış" toplam pazardır, bir oyuncunun payı ~%5-15
- İşlem öncesi GW2BLTC'den güncel fiyat MUTLAKA kontrol et

---

## Bölüm 2: Nexus Addon — GW2 TP Karar Destek Aracı

### Platform: Raidcore Nexus
Kullanıcı zaten Nexus kurulu ve aktif kullanıyor. Addon bu framework üzerine inşa edilecek.

- **Framework**: Nexus (Raidcore) — DX11 proxy DLL, oyun içi yönetim (CTRL+O)
- **Dil**: C++ (resmi SDK)
- **UI**: Dear ImGui v1.92.7 (Nexus tarafından sağlanır)
- **ToS**: Nexus ArenaNet Third Party Policy'ye uygun, kaynak kodu açık/denetlenebilir
- **HTTP**: WinHTTP (Windows native, Schannel TLS, sıfır ek bağımlılık — FarmingTracker ile aynı)
- **Referans addon**: FarmingTracker (xliviax) — GW2 API + API key kullanımı kanıtlanmış
- **Kullanım**: Kişisel — sadece sen kullanacaksın, dağıtım planı yok
- **Başlangıç sermayesi**: ~100g (copper-tier + bazı silver-tier itemler)
- **Crafting**: Şu anda 400+ disiplin yok — Crafting modülü (Faz 4) düşük öncelik
- **C++ + VS 2022**: Kurulu ve hazır
- **Durum**: Plan tamamlanıyor, kodlamaya henüz başlanmadı

### Dil Kararı: Neden C++?
- Tek kanıtlanmış Nexus + API key referans (FarmingTracker) C++ ile yazılmış
- Resmi template C++ — en çok örnek ve topluluk desteği burada
- std::thread yeterli (async runtime gereksiz — 5dk'da 1 HTTP istek)
- Rust/nexus-rs alternatif ama daha az topluluk örneği var

### HTTPS Stack Kararı: WinHTTP (Kesinleşti)
FarmingTracker kaynak kodu incelendi → **WinHTTP** kullanıyor. Kanıtlanmış, sıfır ek bağımlılık.
- `winhttp.h` + `#pragma comment(lib, "winhttp.lib")`
- Schannel TLS (Windows native) — OpenSSL gereksiz
- Pattern: `WinHttpOpen → WinHttpConnect → WinHttpOpenRequest(SECURE) → Send → Receive → Read loop`
- Threading: `std::thread` + `atomic<bool>` stop flag + `condition_variable` + `join()` on Shutdown

### ÖNEMLİ: Sınırlamalar, Riskler ve Çözümleri
| Sınırlama / Risk | Gerçek | Çözüm |
|-------------------|--------|-------|
| API read-only, emir veremez | Evet | Addon KARAR VERİR, sen TP'de tıklarsın |
| Günlük satış hacmi API'de yok | Evet | `/listings` quantity diff ile kaba tahmin (watchlist itemleri); kesin hacim için GW2BLTC referans |
| Rate limit | 300 burst, 5/sn refill, max 200 ID/istek (wiki doğrulandı) | 30 item = 1 istek/poll. 5dk aralıkla = 1 istek/5dk. Limit sorun değil |
| ArenaNet ban riski | Nexus ToS uyumlu | Sadece resmi API + ImGui overlay, memory reading YOK |
| **OYUN DONMASI RİSKİ** | Nexus ImGui callback render thread'de çalışır | **HTTP ASLA render callback'te çağrılmaz**. Worker thread poll eder → snapshot'ı mutex altında yazar → render sadece okur. Bu #1 Nexus addon hatası. |
| **API downtime** | Her patch'te ve rastgele kapanır | Son geçerli fiyatları cache'le, zamandamgası göster ("5dk önce" / "47dk önce — VERİ ESKİ"), hata durumunda hang etme |
| **Undercut relist maliyeti** | Satış emrini iptal etmek %5 listeleme ücretini KAYBEDER | hold_net, relist_net, relist_maliyeti + kuyruk bekleme süresi gösterir. Karar kullanıcıda — relist_net < 0 ise KIRMIZI uyarı |
| **İşlem geçmişi 90 gün** | Wiki doğrulandı: history sadece son 90 gün | P&L modülü 1. günden itibaren lokal JSON/SQLite'a kaydetmeli, yoksa eski veriler kaybolur |
| **Mystic Forge reçeteleri** | `/v2/recipes`'te YOK (MF reçeteleri API dışı) | T5→T6 promotion, ecto→dust gibi MF reçetelerini hardcode et. Philosopher's Stone = spirit shard (TP'de satılmaz) → kullanıcıdan shard değeri girişi iste veya 0 kabul et |
| **Addon Unload crash** | Nexus hot-reload yapar; worker thread HTTP ortasındayken Unload gelirse game crash | Atomic stop flag + kısa HTTP timeout (5-10s) + `join()` Unload'da + static state reset (Load tekrar çağrılabilir) |
| **Sadece oyun açıkken çalışır** | Addon = DLL, GW2 kapanınca durur | Fiyat geçmişi ve hacim tahmini boşluklu olacak — kabul edilen kısıtlama. Gelecekte: companion background service |
| **Item isim→ID eşlemesi** | `/v2/items` name search desteklemez | Watchlist'e item eklerken: ya lokal name→ID index (tek seferlik tüm ID'ler indir) ya da ID ile ekleme. Varsayılan watchlist item ID'leri hardcode |
| **Vendor/karma malzemeleri** | Crafting reçetelerinde TP dışı malzemeler var (ör. Thermocatalytic Reagent) | Bilinen vendor fiyatlarını hardcode et veya "manuel fiyat gir" alanı ekle |
| **MF T5→T6 ortalama 6.05 doğrulanmamış** | Kaynaklar farklı değerler veriyor | Faz 4'te gw2lunchbox'tan güncel yield ortalamasını al, hardcode etmeden önce doğrula |
| **ImGui ABI uyumu** | Farklı ImGui versiyonu = struct layout crash | Nexus template'in sağladığı ImGui header'ları ile derle, kendi ImGui versiyonunu ekleme |

### Konsept
Oyun içi ImGui overlay — TP açıkken veya kapalıyken çalışır:
- **Flipping Yardımcısı**: Watchlist itemlerinde canlı kâr/zarar hesabı
- **Crafting Kâr Hesaplayıcı**: Malzeme al → craft et → sat döngüsünde kâr analizi
- **Kişisel P&L**: Kendi işlem geçmişinden gerçek kâr/zarar raporu
- **Undercut Uyarısı**: Açık satış emirlerinde birisi altına girerse bildirim
- **Spread Alert**: İzlenen itemlerde kârlı fırsat oluşunca bildirim

### GW2 API Endpoint'leri
| Endpoint | Auth | Kullanım |
|----------|------|----------|
| `/v2/commerce/prices?ids=...` | Yok | Anlık buy/sell fiyatları |
| `/v2/commerce/listings/{id}` | Yok | Emir defteri derinliği |
| `/v2/commerce/transactions/current/buys` | API Key | Açık alış emirlerim |
| `/v2/commerce/transactions/current/sells` | API Key | Açık satış emirlerim |
| `/v2/commerce/transactions/history/buys` | API Key | Geçmiş alışlarım |
| `/v2/commerce/transactions/history/sells` | API Key | Geçmiş satışlarım |
| `/v2/items?ids=...` | Yok | Item meta (isim, ikon, tip) |
| `/v2/recipes/search?output={id}` | Yok | Crafting reçeteleri |
| `/v2/recipes/{id}` | Yok | Reçete detayı (malzemeler) |
| API Key oluşturma | — | account.arena.net/applications → `account` + `tradingpost` scope |

### Nexus Addon Teknik Mimari

```
GW2-TP-Assistant.dll (Nexus Addon)
├── Core
│   ├── HttpClient (cpp-httplib veya WinHTTP)
│   ├── GW2ApiClient (tüm endpoint sarmalayıcıları)
│   ├── ProfitEngine (%15 vergi hesabı, spread analizi)
│   └── ConfigManager (API key, watchlist, ayarlar — JSON)
├── Modüller
│   ├── FlipTracker (watchlist + spread alert + polling)
│   ├── CraftingCalc (reçete çözümleme + malzeme maliyet + kâr)
│   ├── PnLTracker (kişisel işlem geçmişi + kâr/zarar)
│   ├── UndercutDetector (açık emirler vs güncel listings)
│   └── PriceHistory (kendi topladığı fiyat verisi, lokal SQLite/JSON)
├── UI (ImGui)
│   ├── MainWindow (tab'lı ana pencere)
│   ├── FlipPanel (watchlist tablosu, renk kodlu kâr/zarar)
│   ├── CraftingPanel (reçete seçici, malzeme listesi, kâr özeti)
│   ├── PnLPanel (işlem geçmişi tablosu, toplam kâr)
│   ├── AlertPanel (son uyarılar listesi)
│   └── SettingsPanel (API key girişi, polling aralığı, watchlist düzenleme)
└── Nexus Entegrasyonu
    ├── Quick Access Bar ikonu
    ├── Keybind (toggle overlay)
    └── Settings (Nexus ayarlar menüsünde)
```

### Özellik Detayları

#### 1. Flip Tracker (Ana Özellik)
- Watchlist'e eklenen itemleri düzenli aralıklarla poll et (varsayılan: 5dk)
- Her item için: buy price, sell price, spread %, birim kâr (vergi sonrası)
- Renk kodu: yeşil (ROI >%5), sarı (ROI %0-5), kırmızı (ROI <0 — zarar)
- Spread alert: Belirlenen eşiğin üstüne çıkınca bildirim (Nexus Alerts_Notify varsa o, yoksa ImGui popup)
- **Varsayılan watchlist**: Copper-tier itemleri (Coarse Sand, Green Wood Plank, Walnut vb.)
- Hacim tahmini: `/listings` quantity diff ile kaba tahmin (her poll'da quantity farkı = satış tahmini). Kesin hacim için GW2BLTC referans

#### 2. Crafting Kâr Hesaplayıcı
- `/v2/recipes/search` ile item'ın reçetesini bul
- Tüm malzemelerin canlı TP fiyatını çek
- Toplam maliyet vs satış fiyatı (vergi dahil) karşılaştır
- Özel: Time-gated malzemeler (Deldrimor Steel vb.) için günlük kâr hesabı
- Özel: Mystic Forge T5→T6 promotion kâr hesabı (ort. [doğrulanacak] çıkış)

#### 3. Kişisel P&L (Kâr/Zarar Raporu)
- `/transactions/history` üzerinden gerçekleşmiş işlemler
- Item bazında: kaç adet aldın, kaç sattın, ortalama alış/satış, net kâr
- Oturum bazında ve toplam kâr/zarar
- Vergi kesintisi ayrı gösterilir

#### 4. Undercut Dedektörü
- `/transactions/current/sells` — senin açık satış emirlerin
- `/listings/{id}` — güncel emir defterinin tepesi
- Senin fiyatının altına birisi girerse → alert + 3 bilgi göster:
  ```
  hold_net       = eski_fiyat × 0.85 − alış_fiyat     (beklersen kâr)
  relist_net     = yeni_fiyat × 0.85 − alış − eski × 0.05  (relist edersen kâr)
  relist_maliyeti = hold_net − relist_net               (relist'in bedeli)
  ```
  Ayrıca `/listings/{id}`'den: senin altındaki birim sayısı ÷ tahmini günlük hacim
  = bekleme süresi tahmini ("altında 340 birim, ~12K/gün → ~40dk bekle")
  - `relist_net < 0` → KIRMIZI "relist zarar, BEKLE"
  - `relist_net > 0` → üç sayıyı göster, karar kullanıcıda
  - Yüksek hacimli itemlerde (Coarse Sand vb.) beklemek genelde doğru
  - Düşük hacimli itemlerde (Bag of Radiant Energy, 335/gün) relist gerekebilir
  - **Uyarı**: Formül satış olasılığını içermez — kuyruktaki birim sayısı karar girdisi

#### 5. Salvage Kâr Hesaplayıcı (Ek)
- Level 68+ Rare ekipman fiyatı vs (0.9 × ecto fiyatı × 0.85)
- Kit maliyeti dahil
- Kârlıysa yeşil, değilse kırmızı

### Mimari Kural: Render Thread Güvenliği
```
[Worker Thread]                    [Render Thread (ImGui)]
   │                                      │
   ├─ 5dk'da bir HTTP poll ──┐            │
   │                         │            │
   ├─ JSON parse             │            │
   │                         │            │
   ├─ ProfitEngine hesapla   │            │
   │                         ▼            │
   └─ mutex.lock() ──► snapshot yaz       │
                                          │
                        mutex.lock() ◄──── render sadece OKUR
                        snapshot oku ────► ImGui tablo çiz
```
**KURAL**: HTTP, JSON parse, hesaplama → SADECE worker thread.
Render callback → SADECE mutex altında snapshot oku + ImGui çiz.
Bu kuralı bozmak = oyun donması.

---

## Bölüm 3: Yapılacaklar (TODO)

### Faz 0: Hazırlık
- [x] API doğrulandı: history 90 gün sınırlı (wiki), transactions verisi 5dk cache (wiki)
- [x] API key scope: `account` + `tradingpost` gerekli (wiki)
- [x] VS 2022 + C++17 ortamı kurulu
- [x] API rate limit doğrulandı (wiki Best Practices): 300 burst, 5/sn refill (300/dk), max 200 ID/istek
- [ ] API key oluştur: account.arena.net/applications → `account` + `tradingpost` scope
- [ ] `/v2/commerce/prices?ids=19976` test çağrısı — response yapısını doğrula
- [ ] Nexus addon template'i kur: github.com/RaidcoreGG/GW2Nexus-AddonTemplate
- [ ] Template README'den ImGui ABI versiyonunu doğrula (Nexus'un sağladığı header'lar ile derlenmeli — versiyon uyuşmazlığı = crash)
- [x] FarmingTracker kaynak kodu incelendi: WinHTTP + atomic stop + CV + join pattern
- [ ] nlohmann/json ekle
- [x] Watchlist varsayılan item ID'leri (API ile doğrulandı):
  | Item | ID |
  |------|----|
  | Pile of Coarse Sand | 71641 |
  | Green Wood Plank | 19710 |
  | Powdered Rose Quartz | 86269 |
  | Congealed Putrescence | 83757 |
  | Walnut | 12250 |
  | Seasoned Wood Log | 19727 |
  | Corsair Tuning Crystal | 86287 |
  | Brilliant Opal Jewel | 24542 |
  | Bag of Radiant Energy | 71730 |
  | Hard Wood Log | 19724 |
  | Eye of Kormir | 83103 |
  | Glass of Buttered Spirits | 77667 |
  | Super Loot Bag | 97982 |
  | Candy Corn Almond Brittle | 36077 |
- [ ] `git init` + `.gitignore` (config JSON'da API key var — repo'ya girmemeli)

### Faz 1: Temel Altyapı ✅
- [x] Console harness (test_harness.exe) — ProfitEngine, ConfigManager, GW2ApiClient test edildi
- [x] Nexus addon iskelet (entry.cpp) — DLL yapısı, ImGui render callback
- [x] AddonUnload güvenliği — atomic stop flag + CV mutex + thread join + static state reset
- [x] Worker thread — HTTP worker'da, render sadece snapshot okur
- [x] Mutex + snapshot pattern
- [x] HttpClient (WinHTTP, 10s timeout)
- [x] GW2ApiClient — prices, items, transactions (206 Partial Content destekli)
- [x] API hata yönetimi — son geçerli veri cache, zamandamgası gösterim
- [x] ConfigManager — API key, watchlist, JSON dosya, dizin otomatik oluşturma
- [x] ProfitEngine — iki ayrı ücret (min 1c), spread analizi, relist maliyet hesabı

### Faz 2: Flip Tracker + Alert ✅
- [x] Watchlist UI — tablo + item silme butonu (X) + item ekleme (ID ile)
- [x] Polling döngüsü — worker thread, ayarlanabilir aralık (slider)
- [x] Spread hesabı ve renk kodlu gösterim (yeşil >%5, sarı %0-5, kırmızı <0)
- [x] Spread alert — Nexus GUI_SendAlert + hysteresis dedup (%20 fire, %10 reset)
- [x] İlk poll sessiz — mevcut durumu kaydeder, alert basmaz
- [x] Alert pencere kapalıyken de çalışır (DrainAlerts render'dan önce)
- [x] Varsayılan watchlist (9 item, ID'ler hardcode)
- [x] Item ekleme — placeholder isim ("Item #ID"), worker otomatik çözer
- [x] ForcePoll — Yenile butonu + ekle/sil/sıfırla sonrası anında güncelleme
- [x] Render thread'de HTTP çağrısı YOK (add-by-ID placeholder pattern)

### Faz 3: P&L + Undercut ✅
- [x] Transaction history — sayfalı çekme (?page=N&page_size=200), int64_t ID
- [x] FIFO maliyet hesabı — tarihe göre sıralı (ISO-8601 lexicographic sort test edildi)
- [x] Eşleşmeyen satışlar ayrı gösteriliyor (unmatchedSellQty), kâr hesabından hariç
- [x] Lokal veri birikimi — map<int64_t, TR> ile merge, aynı ID tekrarlanmaz (test edildi)
- [x] HTTP render thread'den **tamamen** Worker'a taşındı (RequestPnL/RequestUndercut)
- [x] Item isim çözümleme — batch /v2/items, Worker thread'de
- [x] Undercut alış fiyatı — P&L'den FIFO avg cost kullanılıyor
- [x] Per-item "yoksay" toggle (checkbox) + ignored items toplam kârdan hariç
- [x] Tab'lı UI: Flip Tracker | Kar/Zarar | Undercut
- [x] Undercut alert — Nexus GUI_SendAlert ile bildirim
- [x] test_pnl.cpp: 6 unit test (basic FIFO, sort, reversed order, unmatched, ignored, merge)

### Faz 4: Crafting Hesaplayıcı (DÜŞÜK ÖNCELİK — şu anda 400+ disiplin yok)
Crafting disiplinleri yükseltildiğinde devreye girer.
Not: Time-gated crafting 450+ gerektirir. 0→500 leveling maliyetini gw2efficiency
crafting calculator ile hesapla, ~1-3g/gün kâr ile karşılaştır.
- [ ] Normal reçeteler: `/v2/recipes/search` + `/v2/recipes/{id}` ile çözümleme
- [ ] Malzeme maliyet hesabı (canlı fiyatlarla)
- [ ] Crafting kâr paneli (ImGui)
- [ ] Time-gated malzemeler (Deldrimor Steel vb.) — günlük kâr hesabı
- [ ] Mystic Forge reçeteleri — API'de YOK, hardcode edilmeli:
  - T5→T6 promotion: 50 T5 + 1 T6 + 5 Dust + 5 Phil. Stone = ort. [gw2lunchbox'tan doğrula] T6
  - Ecto→Dust: 1 ecto = ort. 1.84 dust
  - Phil. Stone = spirit shard (TP'de satılmaz) → kullanıcıdan shard değeri iste veya 0 kabul et
- [ ] Vendor/karma malzemeleri — bilinen vendor fiyatlarını hardcode et (ör. Thermocatalytic Reagent)
- [ ] Crafting disiplin kontrolü — `/v2/characters/:id/crafting` → `characters` scope (opsiyonel)

### Faz 5: Ek Özellikler
- [ ] Salvage kâr hesaplayıcı
- [ ] Fiyat geçmişi + hacim tahmini (lokal depolama, listings farkından Sold/Bought proxy, grafik)
- [ ] Türkçe UI (ana dil)

#### Faz 5a: Emir Defteri Derinliği ✅ (10 Eyl 2026)
**Neden:** Bugünkü analizin açık bıraktığı tek soru "alış emrim ne kadar bekler?" Prices endpoint'i sadece
en iyi fiyatı verir; kuyruk, kademe ve inceliği vermez. Radiant tuzağı listings'te çıplak görünüyordu
(alış: 52s62c×38 → 52s40c×25 → **27s92c**×20 — 3. kademe %47 aşağıda).

**Kaynak:** `GET /v2/commerce/listings?ids=…` (≤200 id, auth yok, canlı doğrulandı). Item başına
`buys[]` fiyat azalan, `sells[]` fiyat artan; her kademe `{unit_price, quantity, listings}`.
Poll başına 2 çağrı (prices + listings) — 300 burst limitine göre önemsiz. Payload item başına 100+
kademe olabilir → worker'da parse et, snapshot'a sadece top-5 kademe + toplamlar koy.

**Veri modeli (`BookAnalyzer.h` → `BookStats`, `WatchlistSnapshot::Entry::book`):**
```
struct BookLevel { int price; int qty; int listings; };
struct OrderBook { int itemId; vector<BookLevel> buys /*azalan*/, sells /*artan*/; };
struct BookStats {
  int buyQtyAtTop, sellQtyAtTop;      // en iyi fiyattaki birim = aynı fiyata girersem önümdeki kuyruk
  int buyQtyWithin5, sellQtyWithin5;  // %5 bandındaki birim = gerçekçi rekabet
  int buyQtySum, sellQtySum;          // tam merdiven toplamı (≈ prices.quantity — doğrulandı)
  int buyLevels, sellLevels;
  bool thinBook;     // 2×orderQty'lik kümülatif alış desteği top'un %10+ altında ya da merdiven bitiyor
  bool depthCovers;  // iki taraf da orderQty'yi karşılıyor (anlık flip mümkün)
  int instantBuyCost, instantSellRev, instantFlip;  // tam merdivenden süpürme (VWAP), satış vergili
};
Entry: bool hasBook, bookStale; BookStats book; vector<BookLevel> buyTop, sellTop; // top-5 sadece görüntü
```

**DÜZELTME (advisor, tasarım incelemesi):**
1. `thinBook` "2. kademe %10 aşağıda" tanımı Radiant'ı **kaçırıyordu** (2. kademe 52s40c = %0.4 aşağı; uçurum
   3. kademede). Yeni tanım pozisyon boyutuna bağlı: kümülatif alış adedi `2×orderQty`'ye ulaştığı fiyat
   top'un %10+ altındaysa (ya da merdiven bitmeden ulaşamıyorsa) → İNCE. Radiant (38 → 76 gerekli: 38+25=63 →
   3. kademe 27s92c = %47) ✓, Compote (250 → 500: 149+772=921 @122c, %0.8) ✗, Badge (125 → 250: 400 @1593, %0.06) ✗.
2. VWAP top-5'ten hesaplanırsa yanlış (Compote satış top-3 = 80 birim, emir 250). Tüm istatistikler worker'da
   **tam merdiven** üzerinde hesaplanıp skaler saklanıyor; kesilmiş top-5 yalnızca tooltip için.
   Merdiven emri karşılamıyorsa `depthCovers=false` → "defter emir boyutunu bile karşılamıyor".
3. `bookStale` için açık carry-forward: `PollOnce` başında eski snapshot kopyalanır; listings başarısızsa
   entry'nin defter alanları eski entry'den kopyalanır + `bookStale=true`. Yorum değil, davranış.
4. `GetListings` 206 Partial Content kabul eder (tek kötü ID tüm defterleri boşaltmasın).
5. Faz 3'te açık kalan `UndercutInfo::unitsBelow` artık dolu: Σ satış adedi (fiyat < benim fiyatım) →
   Undercut sekmesinde "Altında: N birim" = benim listem sıraya gelmeden önce satılması gereken miktar.

**Kullanıcıya gösterilen (10 sütun korunarak — 11. sütun 950px'te sıkışıyordu):**
- **Talep** hücresi: `149 / 55.8K` = top kuyruk / toplam. Renk: kuyruk ≤ ½·orderQty yeşil, ≤ 3·orderQty
  sarı, üstü kırmızı; `bookStale` ise soluk. Hover → alış merdiveni (5 kademe: fiyat × adet × emir sayısı),
  "+N kademe daha", %5 bandı, ipucu "+1c teklif → kuyruk 0".
- **Arz** hücresi: `41 / 1.2K` = en iyi fiyattaki / toplam. Hover → satış merdiveni + "%5 bandındaki
  birimler seni geri undercut eder".
- **Durum** → **İNCE** (turuncu, ALIM RİSKLİ'den sonra, KARLI'dan önce). Tooltip 2×orderQty sayısını verir.
- **Kâr/Emir** tooltip → "Anlık flip (sabırsız, defterden süpür): −X" ya da "Defter N birimi karşılamıyor".
  Sabırlı kâr ile yan yana → sabrın neden şart olduğunu rakamla gösterir.

**Mimari:** aynı worker/snapshot deseni; poll başına 2 çağrı (prices + listings). Render thread'de sıfır HTTP.

**Test sonuçları (10 Eyl 2026):**
- `test_book` 6/6: Radiant İNCE ✓, Compote depthCovers=false + instantBuy=315×41+316×20+317×19 ✓,
  Badge derin + anlık flip negatif ✓, kısa merdiven ✓, boş defter ✓, TopN ✓.
- `test_worker` [3b]: 17/17 defter geldi; `buyTop[0].price` ≈ `price.buyPrice` 17/17 (%5 içinde);
  `buyQtySum` ≈ `price.buyQty` 17/17 (%10 içinde; ör. 32067/32060) → **prices.quantity = toplam derinlik**
  varsayımı sabitlendi, Talep/Arz'daki "kuyruk" bilgisi gerçekten yeni. 1 İNCE: Potent Master Tuning
  Crystal (top 18s × 10 birim). Regresyon: ForcePoll, dedup, resolve, remove, Stop 0ms hepsi geçti.
- `test_pnl` 6/6 değişmedi.

- [x] GW2ApiClient::GetListings(ids) → std::vector<OrderBook> (200/206)
- [x] BookAnalyzer (saf, header-only): Analyze(book, orderQty) + TopN + test_book.cpp
- [x] Worker::PollOnce: listings çağrısı + Entry doldurma + carry-forward/bookStale
- [x] UI: Talep/Arz "kuyruk / toplam", hover merdivenleri, İNCE etiketi, anlık flip tooltip
- [x] Undercut unitsBelow
- [x] test_worker [3b] genişletme
- [ ] Oyun içi doğrulama (kullanıcı): DLL kopyala → Talep hücresine hover → merdiven görünmeli
