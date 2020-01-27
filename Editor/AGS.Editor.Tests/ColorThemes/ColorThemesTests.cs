﻿using System;
using System.Linq;
using AGS.Editor.Preferences;
using NSubstitute;
using NUnit.Framework;

namespace AGS.Editor
{
    [TestFixture]
    public class ColorThemesTests
    {
        private IAppSettings _settings;
        private IColorThemes _themes;

        [SetUp]
        public void SetUp()
        {
            _settings = Substitute.For<IAppSettings>();
            _themes = new ColorThemes(_settings);
        }

        [Test]
        public void CurrentWillDefaultToDefaultTheme()
        {
            Assert.That(_themes.Current.Name, Is.EqualTo(ColorThemeStub.DEFAULT.Name));
        }

        [Test]
        public void DefaultThemeWillBeAddedToTheThemeCollection()
        {
            Assert.That(_themes.Themes.Any(t => t == ColorThemeStub.DEFAULT), Is.True);
        }

        [Test]
        public void SettingCurrentToNullWillThrowNullReferenceException()
        {
            Assert.Throws<NullReferenceException>(() => _themes.Current = null);
        }

        [Test]
        public void SettingCurrentWillUpdateSettings()
        {
            Assert.That(_settings.ColorTheme, Is.EqualTo(ColorThemeStub.DEFAULT.Name));
            _settings.Received().Save();
        }
    }
}
