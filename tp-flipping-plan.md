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

### Faz 4: Crafting Hesaplayıcı (SIRADA)

**Neden:** Kullanıcının 400+ disiplini yok ama "yapmak gerekiyorsa yaparım" dedi. Addon'un
cevaplamasi gereken soru: "Hangi disiplini kasmalıyım ve günde kaç gold kazanırım?" Time-gated
günlük craft (ascended malzemeler) GW2'de en istikrarlı altin kazanma yolu (~1-3g/gün/disiplin).
Yatırım kararı = leveling maliyeti / günlük kâr = kaç günde amorti.

**Kaynak:** `/v2/recipes?ids=…` (auth yok, 200 ID/istek), `/v2/recipes/search?output=ITEM_ID`.
Reçete yapısı: `{id, output_item_id, output_item_count, min_rating, time_sec (0=normal, >0=time-gated),
disciplines[], ingredients[{item_id, count}], flags[]}`. Fiyatlar: mevcut `GetPrices`.

**Time-gated ascended malzemeler — 4 tier-1 refinement (hepsi 1/gün, 450 rating):**
Bunlar asıl kapılı olanlar; tier-2 ürünler (Deldrimor Steel vb.) bunları TÜKETİR ama kendileri kapılı değildir.
| Tier-1 (kapılı) | Item ID | Disiplinler | → Tier-2 ürün |
|---|---|---|---|
| Lump of Mithrillium | 46745 | Armorsmith, Artificer, Huntsman, Weaponsmith | → Deldrimor Steel Ingot (46742) |
| Spool of Thick Elonian Cord | 46743 | Armorsmith, Huntsman, Leatherworker, Tailor | → Elonian Leather Square (46741) |
| Spool of Silk Weaving Thread | **TBD** | Armorsmith, Leatherworker, Tailor | → Bolt of Damask (46740) |
| Glob of Elder Spirit Residue | 46746 | Artificer, Huntsman, Weaponsmith | → Spiritwood Plank (46744) |
**Jeweler 400'de kalır, 450 reçetesi yok.** Disiplinler reçeteden okunur, hardcode edilmez.
Malzeme listesi API'den çekilir, ASLA hardcode edilmez — yalnızca çıktı item ID'leri sabit (2013'ten beri).

**DÜZELTME (advisor):**
1. İlk plan reçete detaylarını bellekten hardcode ediyordu — API bunu vermek için var. Yalnızca 4 kapılı çıktı
   item ID'si sabit; her şey `/v2/recipes/search?output=ID` + `/v2/recipes?ids=…` ile çekilir.
2. `time_to_craft_ms` animasyon süresi, kapı değil. `RecipeInfo::timeGated` = hardcoded ID set.
3. Kapılı malzemeler recursive çözümlemede **açılmaz** (craft seçeneği yok, günde 1): maliyet = TP alış.
4. Reçete verisi statik → `recipes_cache.json` (atomic yazım), her "Yenile" yalnızca fiyat çeker.
5. Leveling maliyeti hardcode'lanmaz (15-40g, fiyatlarla değişir). Sadece günlük kâr gösterilir +
   "gw2efficiency.com/crafting/calculator" linki. Kullanıcı leveling maliyetini girerse amorti hesaplanır.
6. Account-bound çıktılar: `/v2/items` flags `AccountBound`/`NoSell` → "TP'de satılamaz".
7. `DoCrafting` API key GEREKTİRMEZ (recipes + prices auth yok) — "hangi disiplini kasmalıyım" sorusu
   key girmeden cevaplanır.

**Vendor malzemeleri (hardcode):**
```
Thermocatalytic Reagent  = 150c  (tüm crafting vendor'larında)
Lump of Coal             = 16c   (karma vendor / TP, genelde vendor ucuz)
Jar of Vinegar           = 36c
Jug of Water             = 8c
Bag of Starch            = 64c
Cheese Wedge             = 32c
Glass Mug                = 8c
Ball of Dough            = 48c
```
Liste büyüyebilir → config'te `vendor_prices` JSON objesi, yeni item'lar kullanıcı ya da güncelleme ile eklenir.

**Reçete çözümleme (recursive craft-vs-buy):**
Her malzeme için:
1. TP'de alış fiyatı (`buyPrice` = sabırlı, `sellPrice` = anlık)
2. Eğer malzeme de craftable ise → alt-reçetenin maliyeti
3. min(TP alış, craft maliyeti) seçilir
Bu recursive çözümleme sırasında döngü yakalanmalı (A→B→A); zaten gerçekte yok ama güvenlik.
Derinlik sınırı: 5 kademe (GW2'de daha derin reçete yok).

**Veri modeli (`modules/CraftingCalc`):**
```cpp
struct VendorPrice { int itemId; int copper; std::string name; };

struct RecipeIngredient { int itemId; int count; };
struct RecipeInfo {
    int recipeId; int outputItemId; int outputCount;
    std::vector<RecipeIngredient> ingredients;
    int minRating; int timeSec;  // 0=normal, 86400=1/gün
    std::vector<std::string> disciplines;
};

struct CostBreakdown {
    struct Line { int itemId; std::string name; int count; int unitCost; bool crafted; bool vendor; };
    std::vector<Line> lines;
    int totalCost;          // sum of lines (sabırlı alış)
    int totalCostInstant;   // sell price ile alım
    int sellRevenue;        // NetRevenue(sellPrice) * outputCount
    int profit;             // sellRevenue - totalCost
    int profitInstant;
    double roi;
    bool complete;          // tüm fiyatlar çözümlendi
};

// Saf fonksiyonlar (Worker'da çağrılır):
CostBreakdown CalcRecipeCost(
    const RecipeInfo& recipe,
    const std::map<int, PriceData>& prices,
    const std::map<int, RecipeInfo>& subRecipes,   // craftable ingredients
    const std::map<int, int>& vendorPrices
);
```

**UI — "Crafting" sekmesi (4. tab):**
1. **Günlük Time-Gated** (varsayılan görünüm):
   Disiplin | Ürün | Maliyet | Satış | **Günlük Kâr** | ROI | 450 Leveling (~) | Amorti
   Her satır: malzeme ikonları/isimleri tooltip'te; kâr >= 50s yeşil, < 0 kırmızı.
   **Toplam günlük kâr** (tüm disiplinler) altta.
   Leveling maliyeti: gw2efficiency referansı (hardcode yaklaşık; Chef ~3g, Tailor ~15g, Armorsmith ~10g vb.
   — fiyatlar değişir, addon bunu API'den hesaplamaz, yaklaşık gösterir + "gw2efficiency.com/crafting/calculator
   kullan" tooltip'i).

2. **Reçete Hesaplayıcı** (katlanabilir):
   Item ID gir → `/v2/recipes/search?output=ID` → reçete(ler) göster → recursive maliyet çözümleme.
   Birden fazla reçete varsa hepsini listele (farklı disiplinler/yollar).
   "Craft vs Buy" her malzeme satırında gösterilir.

**Worker entegrasyonu:** `DoCrafting()` poll'da çağrılmaz (ağır — yüzlerce fiyat). Kullanıcı sekmeyi
açtığında veya "Yenile" tıkladığında `RequestCrafting` → Worker time-gated reçeteleri çözer + fiyatları
çeker. Custom reçete araması da Worker'da. Sonuç `CraftingSnapshot` olarak snapshot'a.

**Test:** `test_crafting.cpp` — CalcRecipeCost: basit reçete (3 malzeme, hepsi TP), vendor malzeme,
recursive craft-vs-buy (sub-reçete ucuzsa craft seçilir), recursive buy-vs-craft (TP ucuzsa TP),
eksik fiyat → complete=false, derinlik sınırı, outputCount > 1 normalizasyon.
`test_worker`: DoCrafting API key yokken no-op (reçete API auth istemez ama fiyatlar gerekir).

- [ ] VendorPrices + RecipeInfo + CraftingCalc (saf) + test_crafting
- [ ] GW2ApiClient: GetRecipes(ids), SearchRecipeByOutput(itemId)
- [ ] Worker: DoCrafting, RequestCrafting, CraftingSnapshot (time-gated + custom)
- [ ] UI: Crafting sekmesi (time-gated tablo + reçete hesaplayıcı)
- [ ] test_worker genişletme; sürüm 0.6

### Faz 5: Ek Özellikler
- [ ] Salvage kâr hesaplayıcı
- [ ] Hacim tahmini → **Faz 5b** (aşağıda); fiyat geçmişi grafiği sonraya
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

#### Faz 5b: Hacim Tahmini — Bought/Sold Proxy ✅ kod tamam (10 Eyl 2026), oyun içi veri birikimi bekliyor
**Neden:** Araştırmanın tüm sıralaması GW2BLTC'nin günlük Bought/Sold sayılarına dayandı; API bunu
vermiyor. Radiant tuzağı (Bought 2/gün) yalnızca dış siteden görülebiliyordu. Faz 5a defteri **anlık**
gösteriyor; 5b **zaman içindeki değişimden** doluş hızını türetir → "emrim kaç saatte dolar, devir kaç
gün, kâr/gün ne?" sorusu addon içinde, GW2BLTC'ye bakmadan cevaplanır. Bu, araştırmadaki
"kâr/emir × devir" hedef metriğini ölçülebilir kılar.

**Yöntem (GW2BLTC ile aynı aile — sayılar karşılaştırılabilir):** ardışık iki listings snapshot'ı fiyat
kademesi bazında karşılaştır (kademe anahtarı = unit_price; API zaten fiyata göre toplar).
- **Bought** (alış emirleri dolduruluyor) = önceki snapshot'ın **alış** kademelerinde azalan adet toplamı,
  yalnızca önceki en iyi alışın %5 bandında (derin emir iptalleri sayılmasın; anlık satan en iyi alışa vurur,
  bitince bir alt kademeye kayar — bant bunu kapsar).
- **Sold** (listeler satın alınıyor) = önceki **satış** kademelerinde azalan adet toplamı, önceki en iyi
  satışın %5 bandında.
- Artışlar sayılmaz. İptal ve relist, dolumdan ayrılamaz → **üst sınır tahmini**; UI'da "≈" ve tooltip'te
  açıkça yazılır. Tuzak tespiti için üst sınırın bile düşük çıkması yeterli kanıt (Radiant).
- Aralık dt > max(15 dk, 3×poll) ise (oyun kapalıydı, uyku) delta **ve** zaman atılır.
- API cache aynı veriyi döndürürse sıfır delta + dt sayılır; uzun vadede ortalama doğru.
- Sınır: yalnızca oyun açıkken veri toplanır → tahmin "benim oynadığım saatlerin" hızıdır (tooltip'te not).

**Veri modeli:** `modules/VolumeTracker` — item başına saatlik kova `{epochHour, bought, sold, observedSec}`,
son 168 saat (7 gün), `volume_history.json` (addon dizini, temp+rename ile atomik yazım, her poll).
Worker bellekte item başına son tam merdiven + steady_clock zamanı tutar (snapshot'a girmez; 17 item ×
~100 kademe = önemsiz). `Estimate(itemId)` → `{boughtPerDay, soldPerDay, observedSec, ok}`;
ok = observedSec ≥ 2 sa; 2–6 sa "düşük güven" (soluk), ≥ 6 sa normal.

**Türetilenler (Entry):**
- `fillHours = orderQty / (boughtPerDay/24)` — +1c teklifle kuyruk atlanır varsayımı; tooltip'te
  kuyruklu varyant `(buyQtyAtTop + orderQty) / saatlik`.
- `sellHours = orderQty / (soldPerDay/24)` (top −1c listeleme varsayımı).
- `cycleHours = fill + sell`; `profitPerDay = profitPerOrder × 24 / cycleHours` (sermaye slotu başına).
- `sharePct = orderQty / soldPerDay` — günlük hacimdeki payım; eski "%10 emilim" varsayımı artık ölçülür.

**UI:** 11. sütun **Devir** ("~5 sa" / "~2.3 gün" / "—"), renk < 24 sa yeşil, < 72 sa sarı, üstü kırmızı;
sıralanabilir (veri yoksa sona). Tooltip: alış ≈ X sa (≈B/gün alınıyor) · satış ≈ Y sa (≈S/gün satılıyor) ·
pay %Z · kâr/gün ≈ G · veri N sa · "relist/iptal dahil, üst sınır". Pencere max genişliği 950 → 1150;
tablo `Hideable` (başlığa sağ tık → ROI/Kar gizlenebilir). **Durum:** hacim verisi varsa fiyat-heuristiği
(`BuySideRisky`) yerine ölçüm: fillHours > 7 gün → ALIM RISKLI (ölçülen, tahmini ezer).

**DÜZELTME (advisor, tasarım incelemesi):**
1. **Durum ezme kuralı tehlikeliydi:** 2 sa veride tek 20'lik dolum → 240/gün ekstrapolasyonu → Radiant
   "güvenli" görünür ve bugün onu yakalayan fiyat-heuristiği susturulurdu. Yeni kural: ölçüm riski **her
   güvende ekleyebilir** (fillHours > 7 gün ya da DOLMUYOR → ALIM RISKLI), heuristiğin bayrağını **yalnızca
   ≥ 6 sa** veride kaldırabilir. 2–6 sa arası: riskli = heuristik ∨ ölçüm.
2. **"Ölçülen sıfır" ≠ "veri yok":** 6 saatte 0 dolum bu fazın üreteceği en güçlü sinyal; ayrıca
   `fillHours` sıfıra bölünürdü. Üç durum: `!ok` → "—" gri ("N sa veri, 2 sa gerekli"); `ok ∧ hız=0` →
   **DOLMUYOR** kırmızı ("N saatte 0 dolum", hangi taraf); `ok ∧ hız>0` → "~X sa". Sıralama artan: sonlu →
   DOLMUYOR → veri yok. Sıfır-hız yolu unit testte.
3. **dt için `system_clock`**, steady_clock değil: MSVC steady_clock = QPC; S3 uykuda davranışı garanti
   değil — 3 saatlik uyku "5 dk" okunursa 3 saatlik işlem 5 dakikaya yazılırdı. system_clock + gap kuralı
   hem uykuyu hem saat ayarını yakalar (saat sıçraması = 1 atılan delta). Snapshot timestamp steady kalır.
4. **Bias yönü etiketlenir:** relist Sold'u, +1c savaşı Bought'u şişirir → hacim **üst sınır**, dolayısıyla
   Devir **iyimser (alt sınır)**, kâr/gün **üst sınır**. Tooltip: "en iyi ihtimalle — relist/iptal dolum
   sayılır". "GW2BLTC ile karşılaştırılabilir" → "aynı yöntem ailesi; %5 bant nedeniyle bizimki ≤ GW2BLTC".
   Nüans: "üst sınır" **ince defterlerde** geçerli (tuzak vakası). Likit item'larda aynı 5 dk aralığında
   300 dolup 300 yenilenen kademe sıfıra netleşir → orada tahmin **kabaca hacim**, yönü sabit değil.
5. **`listings` sayacı bedava sinyal:** GW2'de emir kısmen iptal edilemez → kademede adet düştü ama
   `listings` aynıysa bu **dolum** (iptal/relist her zaman listings'i düşürür). ComputeDelta iki sayı döndürür:
   `confirmed` (Σ adet↓, listings değişmemiş) ve `total` (üst sınır). Kovada ikisi de saklanır; tooltip
   "onaylı ≥X/gün · üst sınır Y/gün". Uyarı: aynı kademede iptal(−100, L−1) + yeni emir(+50, L+1) çakışması
   50'lik sahte onaylı dolum gibi görünür → güçlü alt tahmin, kanıt değil. Emri tamamen bitiren dolum da
   listings'i düşürür → onaylı sayı yalnızca kısmi dolumları yakalar (bu yüzden "≥").

**Uygulama notları (advisor):** Durum sütunu no-market dalında açık `TableNextColumn` çağrıları ve
`!hasData` dalında `i < 7` döngüsü 10 sütuna sabit → Devir (indeks 8) eklenince ikisi de güncellenir;
sıralama indeksleri 0–7 geçerli kalır, Devir = case 8. `!booksOk` yolunda prev merdiven **güncellenmez**,
delta hesaplanmaz; sonraki başarılı poll'un dt'si boşluğu kapsar, gap kuralı karar verir. Kova ataması:
delta'nın sayıları **ve** dt'si o poll'un epoch saatine yazılır, bölünmez; Estimate aynı kova kümesi
üzerinden toplar → oran çarpılamaz.

**Test:** `test_volume.cpp` — ComputeDelta: top kısmi tüketim (confirmed), kademe kayboldu + alt kademe
kısmi ({100×50, 99×200} → {99×150} = **100** total, 50 confirmed), yeni yüksek top + artışlar → 0,
bant dışı azalma → 0, satış tarafı bant yönü, boş taraflar, listings değişti → confirmed 0;
Estimate: < 2 sa → ok=false, 6 sa'da 12 bought → 48/gün, **6 sa 0 dolum → ok ∧ 0**; gap kuralı;
Save/Load roundtrip; 168 sa budama. `test_worker`: 2. poll sonrası observedSec > 0, history dosyası var.

**Test sonuçları (10 Eyl 2026):**
- `test_volume` 13/13: kısmi dolum onaylı ✓, kademe kayboldu 100/50 ✓, yeni top/artış 0 ✓, bant dışı 0 ✓,
  satış bandı ✓, boş taraflar ✓, listings değişti → onaylı 0 ✓, 6 sa → 48/gün ✓, 1 sa → ok=false ✓,
  **3 sa 0 dolum → ok ∧ 0** ✓, gap kuralı ✓, 168 sa pencere/budama ✓, save/load + .tmp temizliği ✓.
- `test_worker` [4b]: 2. poll sonrası 17/17 item'da observedSec > 0, `volume_history.json` yazıldı, .tmp yok,
  taze geçmişte vol.ok = 0 satır (uydurma tahmin yok). Regresyon (5a defter, ForcePoll, dedup, resolve,
  remove, Stop 0ms) geçti. `test_pnl` 6/6, `test_book` 6/6 değişmedi.
- DLL v0.4, 11 sütun (Devir indeks 8), pencere max 1150, tablo Hideable.

- [x] VolumeTracker (saf delta + kovalar + JSON, temp+rename) + test_volume
- [x] Worker: prev merdiven (system_clock), gap kuralı, Record, Prune, Save; Entry türetilenleri
- [x] UI: Devir sütunu üç durum (— / DOLMUYOR / ~X) + tooltip, Hideable, genişlik 1150; Durum kuralı
      (ölçüm riski her güvende ekler, heuristiği yalnızca ≥ 6 sa kaldırır)
- [x] test_worker [4b] genişletme
- [ ] Oyun içi: ≥ 2 sa oyun sonrası Devir dolmaya başlar (2–6 sa soluk, ≥ 6 sa normal); Radiant/Opal'i
      izleme listesine geçici ekleyip DOLMUYOR'un çıktığını gör → yöntemin doğrulaması

### Faz 6: Emir Takibi — Outbid, Dolum ve Satış Bildirimleri ✅ kod tamam (11 Eyl 2026), oyun içi doğrulama bekliyor
**Neden:** Alsat döngüsü: alış emri ver → (outbid? yeniden teklif) → **doldu** → listele → (undercut?
relist) → **satıldı** → altını topla. Addon bugün yalnızca undercut'ı (elle "Kontrol Et") görüyor. Döngünün
diğer üç olayı — alış emrim geçildi, alış emrim doldu (listele!), listem satıldı — TP'yi açmadan
görünmüyor. Hepsi mevcut API + mevcut `tradingpost` scope ile; otomasyon değil, bildirim + karar desteği.

**Kaynaklar (auth, hepsi istemcide var):**
- `current/buys` → açık alış emirlerim `{id, item_id, price, quantity, created}`; `current/sells` → listelerim.
- `history/buys` / `history/sells` → tamamlananlar, en yeni önce, `purchased` zamanlı. Delta tespiti için
  **yalnızca sayfa 0** (200 en yeni) çekilir → `FetchTransactionsPage(path, page)` eklenir; mevcut
  `FetchTransactions` (50 sayfaya kadar) P&L'de kalır, her kontrolde 50 çağrı atılmaz.
- Emirlerdeki item'lar için `prices` + `listings` (kuyruk hesabı).
- Kısıt: transactions uçları sunucu tarafında cache'li (dakikalar). Bildirim gecikmesi ≈ kontrol aralığı + cache.

**Mantık (`modules/OrderTracker`, saf ve testlenebilir):**
- **Alış emirleri:** (item, fiyat) gruplanır → `myPrice, myQty, oldestCreated`. Defterden `topBuy`;
  `outbid = topBuy > myPrice`, `outbidBy = topBuy − myPrice`; `aheadQty = Σ alış adedi (fiyat > myPrice)
  + max(0, aynı fiyattaki adet − myQty)` — aynı kademede sıra bilinmez, **"en fazla"** etiketiyle üst sınır.
  Öneri: `topBuy + 1c`'ye çıkarsan yeni kâr = `NetRevenue(mevcut satış) − (topBuy+1)`, ROI, kâr/emir
  (min eşikle renk). 5b entegrasyonu: item hacim geçmişindeyse `beklenenSa = (aheadQty + myQty) /
  (boughtPerDay/24)`; `yaş = now − created`; yaş > 2×beklenen → **GECİKTİ**.
- **Satış listeleri:** tüm listelerim (yalnızca undercut'lar değil): `lowest`, `undercut`, `unitsBelow`
  (Faz 5a), mevcut `CalcRelist` + FIFO ort. maliyet; `beklenenSa = (unitsBelow + myQty) / (soldPerDay/24)`.
- **Dolum/satış tespiti:** son çekilen history sayfa-0 id kümesi saklanır (`seenBuyIds`, `seenSellIds`,
  ≤200 int64). Yeni sayfada kümede olmayan id = yeni olay. **İlk çalıştırma tohumlar, bildirmez.** Id
  monotonluğu varsayılmaz (küme farkı). `orders_state.json` (atomik yazım). Kısmi dolumlar ayrı kayıt
  gelir → **item başına, kontrol başına toplanır**: "DOLDU: 3 parça, toplam 120x X @ 52s → listele
  (satış 1g 30s)"; "SATILDI: 100x Y @ 1g 30s → net +Z" (net = NetRevenue×adet − ort.maliyet×adet,
  maliyet biliniyorsa). Olay sonrası `RequestPnL()`.
- **Bildirim dedup:** OUTBID anahtarı (item, myPrice) — outbid olduğunda bir kez; tekrar en üste çıkınca
  ya da emir kaybolunca sıfırlanır. UNDERCUT aynı desen (bugünkü DoUndercut her çalışmada tekrar bildiriyor —
  periyodik olunca dedup şart). Yeniden başlatmada mevcut outbid bir kez daha bildirir (kabul).

**Zamanlama:** fiyat poll'u ile aynı kadans (`PollOnce` → `DoOrders`, API key varsa) + "Yenile" düğmesi
(`RequestOrders`). Döngü başına ~8 çağrı; 60 sn'de bile 300 burst / 5/s limitine göre önemsiz.
Kullanıcı hızlı bildirim isterse poll aralığını düşürür — tek düğme, ikinci zamanlayıcı yok.

**UI:** "Undercut" sekmesi → **"Emirlerim"**. Başlık: "Son kontrol: X sn · Açık: N alış / M satış ·
Bu oturum: F dolum, S satış" + Yenile.
- **Alış Emirlerim:** Item (kopyalanabilir) | Fiyatım | En İyi Alış | Fark | Önümde (≤) | Adet | Yaş |
  Durum (EN ÜSTTE / OUTBID / GECİKTİ) | Öneri (tooltip: +1c yeni kâr, ROI, kâr/emir; beklenen süre).
- **Satış Listelerim:** Item | Fiyatım | En Düşük | Altımda | Adet | Yaş | Durum (EN DÜŞÜK / UNDERCUT) |
  Relist analizi (mevcut holdNet/relistNet/relistCost tooltip'i).
- **Son Olaylar** (katlanabilir): son 20 dolum/satış, zamanıyla.
- Bildirimler (Nexus alert): OUTBID, UNDERCUT, DOLDU, SATILDI — ayarlarda tek anahtar "Emir bildirimleri".

**DÜZELTME (advisor, tasarım incelemesi):**
1. **DoOrders'ın hata yolu yoktu — burada hata boş sütundan çok daha kötü.** `m_lastOk` tek bayrak; her
   çağrıdan sonra ayrı kontrol şart. Aksi halde (a) boş `current/buys` = "tüm emirler gitti" → dedup
   anahtarları sıfırlanır, API dönünce her outbid yeniden bildirir; (b) boş history sayfası `seenIds`'i `{}`
   yapar → sonraki başarılı çekimde 200 kayıt "yeni dolum" = **200 DOLDU bildirimi**. Kural (5a
   carry-forward ile aynı): herhangi bir çağrı başarısızsa önceki `OrdersSnapshot` korunur, `stale=true`,
   seen kümeleri ve dedup anahtarlarına dokunulmaz. `OrderInputs.ok=false` → `Analyze` state'i değiştirmeden
   `skipped` döner (unit test: seen={a,b}, ok=false → olay 0, seen aynı).
2. **Açılış bildirim fırtınası:** 10 açık emrin 6'sı outbid ise yüklemede 6 Nexus alert'i. Fiyat alarmındaki
   `m_firstPoll` çözümü: ilk DoOrders outbid/undercut anahtarlarını **bildirmeden tohumlar**; sekme mevcut
   durumu gösterir, bildirimler *değişim* için. Ayrıca tür başına, kontrol başına toplama: "OUTBID: 3 emir —
   X, Y, Z" (≤3 isim, sonra "+k"). DOLDU/SATILDI: item başına toplanır; > 3 item ise tek özet satırı.
   Yeniden başlatma sonrası diskteki seen kümesine göre yeni dolum/satışlar (oyuncu yokken olanlar) **bildirir**
   — gerçek yeni bilgi; toplama fırtınayı engeller.
3. **200'lük batch:** yoğun seansta `current/buys ∪ current/sells` 200+ farklı item olabilir; `GetPrices/
   GetListings` batch yapmaz → API 400. DoPnL'deki 200'lük döngü DoOrders'taki üç çağrıda da kullanılır.
   Listings yalnızca açık emir item'ları için (ağır payload); history sayfasındaki item'lar için yalnızca prices.
4. **Olay tetikli P&L artımlı:** DoPnL 50 sayfaya kadar çeker; `pnl_data.json` Start'ta yüklenir ve
   `MergeTransactions` id ile birleştirir → dolum sonrası yenileme için yalnızca sayfa 0 yeter:
   `DoPnL(bool incremental)`. Sekmedeki elle düğme tam çekim kalır.
5. **Küme farkı koruması:** history `purchased` desc sıralı, kayıtlar yalnızca aşağı kayar → sayfa-0 kayması
   neredeyse imkânsız, **200 sınırındaki eş-zaman damgası** hariç. Ek kemer: yeni olay için `purchased ≥ en
   yeni görülen purchased` (time_t karşılaştırma). Bir karşılaştırma, hayalet-DOLDU vakasını öldürür.
6. **GECİKTİ eşiği:** `beklenen` üst-sınır `boughtPerDay`'den → iyimser → `2×` likit item'larda relist
   gürültüsüyle erken tetiklenir. **3×** kullanılır; tooltip 5b'nin kendi uyarısını tekrarlar.
7. **Karar — `UndercutDetector` silinir:** OrderTracker aynı girdilerle üst-küme çıktı verir. `DoUndercut`,
   `RequestUndercut`, `GetUndercutSnapshot`, `m_undercutRequested` ve her çalışmada yeniden bildiren eski
   alert yolu kaldırılır; "Undercut" sekmesi "Emirlerim" olur.

**Uygulama notları (advisor):** `OrderTracker::Analyze(OrderInputs, OrderState&)` tamamen saf (BookAnalyzer
gibi) — views + events + alerts döner; tüm testler HTTP'siz. Worker'da `itemId→name` cache (isimler her
5 dk yeniden çekilmesin). DOLDU metni `current/buys`'tan kalan adedi gösterir: "DOLDU: 120x X (emirde 130
kaldı)". `Run()` başındaki ilk `PollOnce()` sonrasına `DoOrders()` da eklenir (ilk kontrol bir aralık
beklemesin). `aheadQty` "en fazla" etiketi doğru — aynı kademede FIFO sırası API'den bilinemez. ISO-8601:
tam `"2026-09-10T20:15:33+00:00"` ve `Z` biçimi test edilir; UTC için `_mkgmtime`.

**Test:** `test_orders.cpp` — outbid + aheadQty; top == myPrice → outbid değil, ahead = aynı kademe − benim;
undercut + unitsBelow + relist; tohum → 0 olay ve outbid varken bile bildirim yok; ikinci turda 2 yeni id
aynı item → 1 toplanmış olay (parts=2) + DOLDU bildirimi; sıra değişimi → 0; eski zaman damgalı yeni id → 0;
dedup: outbid → 1 bildirim, hâlâ outbid → 0, en üste dön → anahtar silinir, tekrar outbid → yeni bildirim;
**ok=false → skipped, state aynı**; ISO-8601 (`+00:00`, `Z`, bozuk → 0); SATILDI net hesabı; state
save/load roundtrip. `test_worker`: API key yokken DoOrders no-op, `hasChecked=false`, çökme yok.

**Test sonuçları (11 Eyl 2026):**
- `test_orders` 10/10: ISO-8601 (+00:00 / Z / bozuk) ✓, outbid + ahead üst sınır (30+20+30=80) + rebid 103 ✓,
  en üstteyken ahead = aynı kademe − benim ✓, undercut + unitsBelow 12 + CalcRelist + beklenen 3.67 sa ✓,
  tohum → 0 olay/0 bildirim (outbid varken bile) ✓, 2 yeni kayıt aynı item → 1 olay (120x, 2 parça,
  "emirde 130 kaldi") ✓, sıra değişimi + eski damgalı yeni id → 0 ✓, SATILDI net = (NetRevenue−maliyet)×adet ✓,
  4 item → tek özet bildirim ✓, dedup döngüsü (1 → 0 → anahtar silinir → 1) ✓, **ok=false → skipped, state
  aynı** ✓, state save/load (keysSeeded sıfırlanır) ✓.
- `test_worker` [7b]: API key yokken `RequestOrders` no-op — hasChecked=0, stale=0, `orders_state.json`
  yazılmadı; 5a/5b/regresyon değişmedi. `test_book` 6/6, `test_volume` 13/13, `test_pnl` 6/6.
- DLL v0.5. `UndercutDetector.h/.cpp` silindi (git rm commit'te).

- [x] GW2ApiClient: `FetchTransactions(path, maxPages)` + `GetHistory{Buys,Sells}Page0`
- [x] OrderTracker (saf Analyze + LoadState/SaveState atomik) + test_orders
- [x] Worker: DoOrders (çağrı başına ok kontrolü, carry-forward, 200'lük batch, isim cache), OrdersSnapshot,
      RequestOrders, `DoPnL(incremental)`, ilk PollOnce sonrası DoOrders; DoUndercut/RequestUndercut kaldırıldı
- [x] UI: Emirlerim sekmesi (Alış Emirlerim 9 sütun, Satış Listelerim 8 sütun, Son Olaylar), ayarlarda
      "Emir bildirimleri" anahtarı + poll tooltip; sürüm 0.5
- [x] test_worker [7b]
- [ ] Oyun içi: açık emirle Emirlerim sekmesi; bir alış emrini bilerek düşük ver → OUTBID; dolum → DOLDU
      bildirimi + Son Olaylar + P&L artımlı yenileme
