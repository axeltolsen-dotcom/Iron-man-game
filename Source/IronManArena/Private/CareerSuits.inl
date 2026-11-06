// Shop suits (Axel, 2026-11-05: "girl suits"): female-proportioned, slimmer armours appended after the generated
// table. Sold in the upgrade shop (U), see Career::ShopSuits(); UCareerSubsystem gives them the feminine armour
// silhouette (narrower shoulders / waist, AIronManCharacter::SetCareerBodyScale). Included inside BuildSuitTable().
		{
			auto Add = [&Out](const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, FLinearColor P, FLinearColor Sec, FLinearColor Acc,
				FLinearColor Glow, ESuitHelmet Helmet, ESuitBackpack Pack, bool bWings, int32 Sp, int32 Ar, int32 En, int32 Rg, int32 Dm,
				ESuitPrimaryWeapon Prim, ESuitSpecialWeapon Spec, EAIPersona AI, ECinematicStyle Cine)
			{
				FSuitDef S;
				S.Id = FName(Id);
				S.Name = FText::FromString(Name);
				S.Mark = FText::FromString(Name);
				S.Description = FText::FromString(Desc);
				S.PrimaryColor = P; S.SecondaryColor = Sec; S.AccentColor = Acc; S.EmissiveColor = Glow;
				S.Helmet = Helmet;
				S.Reactor = ESuitReactor::Round;
				S.Shoulders = ESuitShoulders::None;
				S.Backpack = Pack;
				S.Plating = 0;
				S.bWings = bWings;
				S.SpeedRating = Sp; S.ArmorRating = Ar; S.EnergyRating = En; S.RegenRating = Rg; S.DamageRating = Dm;
				// Same rating -> stat mapping as Tools/generate_suits_and_voices.py.
				S.Speed = 0.7f + 0.06f * Sp;
				S.Armor = 0.6f + 0.08f * Ar;
				S.MaxEnergy = 60.f + 8.f * En;
				S.Regen = 10.f + 2.4f * Rg;
				S.WeaponDamage = 0.6f + 0.08f * Dm;
				S.PrimaryWeapon = Prim;
				S.SpecialWeapon = Spec;
				S.AI = AI;
				S.JarvisIntroLine = S.Description;
				S.CinematicStyle = Cine;
				Out.Add(S);
			};
			Add(TEXT("rescue-mk2"), TEXT("Rescue Mk II"),
				TEXT("A lighter, faster Rescue: violet plating, silver trim and swept wings. Built to get there first."),
				FLinearColor(0.16f, 0.05f, 0.42f), FLinearColor(0.6f, 0.62f, 0.68f), FLinearColor(0.45f, 0.3f, 1.f), FLinearColor(0.55f, 0.75f, 1.f),
				ESuitHelmet::Sleek, ESuitBackpack::Jets, true, 8, 5, 6, 8, 5, ESuitPrimaryWeapon::Sonic, ESuitSpecialWeapon::Shield, EAIPersona::Friday, ECinematicStyle::Nano);
			Add(TEXT("valkyrie"), TEXT("Valkyrie"),
				TEXT("Pearl-white interceptor with teal light lines. The slimmest frame in the hangar and the quickest off the line."),
				FLinearColor(0.78f, 0.8f, 0.82f), FLinearColor(0.05f, 0.32f, 0.34f), FLinearColor(0.2f, 0.9f, 0.85f), FLinearColor(0.3f, 1.f, 0.9f),
				ESuitHelmet::Sleek, ESuitBackpack::None, false, 9, 4, 6, 7, 6, ESuitPrimaryWeapon::Laser, ESuitSpecialWeapon::Overdrive, EAIPersona::Edith, ECinematicStyle::Drop);
			Add(TEXT("aurora"), TEXT("Aurora"),
				TEXT("Rose-gold and white long-range armour. Warm light, cold aim."),
				FLinearColor(0.62f, 0.3f, 0.24f), FLinearColor(0.82f, 0.8f, 0.78f), FLinearColor(1.f, 0.7f, 0.55f), FLinearColor(1.f, 0.65f, 0.5f),
				ESuitHelmet::Classic, ESuitBackpack::Missiles, false, 6, 5, 7, 6, 7, ESuitPrimaryWeapon::Repulsor, ESuitSpecialWeapon::Missiles, EAIPersona::Friday, ECinematicStyle::Assembly);
			Add(TEXT("ironheart"), TEXT("Ironheart"),
				TEXT("Home-built red and gold armour with a heart-shaped reactor housing. Small frame, big punch."),
				FLinearColor(0.45f, 0.015f, 0.02f), FLinearColor(0.78f, 0.5f, 0.1f), FLinearColor(0.9f, 0.7f, 0.25f), FLinearColor(0.4f, 0.9f, 1.f),
				ESuitHelmet::Angular, ESuitBackpack::Jets, false, 7, 6, 6, 6, 8, ESuitPrimaryWeapon::Repulsor, ESuitSpecialWeapon::Unibeam, EAIPersona::Jarvis, ECinematicStyle::Gantry);
			Add(TEXT("nightshade"), TEXT("Nightshade"),
				TEXT("Matte-black slim stealth armour with violet seams. You hear the thrusters after she's gone."),
				FLinearColor(0.015f, 0.012f, 0.02f), FLinearColor(0.08f, 0.03f, 0.12f), FLinearColor(0.35f, 0.1f, 0.6f), FLinearColor(0.6f, 0.25f, 1.f),
				ESuitHelmet::Sleek, ESuitBackpack::None, false, 8, 4, 6, 7, 6, ESuitPrimaryWeapon::Laser, ESuitSpecialWeapon::Cloak, EAIPersona::Edith, ECinematicStyle::Stealth);
			Add(TEXT("deep-space"), TEXT("Deep Space"),
				TEXT("White pressure-sealed armour for orbit and the deep ocean. Runs cold, flies far."),
				FLinearColor(0.82f, 0.83f, 0.84f), FLinearColor(0.25f, 0.27f, 0.3f), FLinearColor(0.9f, 0.45f, 0.08f), FLinearColor(0.7f, 0.9f, 1.f),
				ESuitHelmet::Bulky, ESuitBackpack::Jets, false, 6, 7, 9, 6, 5, ESuitPrimaryWeapon::Frost, ESuitSpecialWeapon::Shield, EAIPersona::Friday, ECinematicStyle::Gantry);
			Add(TEXT("impulse"), TEXT("Impulse"),
				TEXT("Teal and black VR-born armour: thruster pods on every limb, wings for the long boost."),
				FLinearColor(0.01f, 0.18f, 0.2f), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.1f, 0.9f, 0.8f), FLinearColor(0.2f, 1.f, 0.85f),
				ESuitHelmet::Angular, ESuitBackpack::Jets, true, 10, 4, 7, 6, 6, ESuitPrimaryWeapon::Repulsor, ESuitSpecialWeapon::Overdrive, EAIPersona::Jarvis, ECinematicStyle::Drop);
		}
