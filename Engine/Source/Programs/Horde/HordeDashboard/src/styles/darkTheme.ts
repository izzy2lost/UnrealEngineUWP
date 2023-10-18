import { createTheme } from '@fluentui/react';
import { HordeTheme, HordeThemeExtensions } from './themeTypes';


// text colors
const textColor = "#FFFFFF";
const linkColor = "#55B7FF";
const linkColorHovered = "#AADBFF";
const hilightColor = "#363636";

// Horde specific color extensions
const hordeDarkTheme: HordeThemeExtensions = {
    darkTheme: true,
    // Top nav background color
    topNavBackground: "#242729",
    // Breadcrumb area background color
    breadCrumbsBackground: "#1B1D1E",
    // Panel and modal content background color
    contentBackground: "#1F1F1F",
    // Neutral background color, used for site margins, and hinting such as in detail list rows
    neutralBackground: "#101010",
    // divides sections
    dividerColor: "#2A2A2A",
    // Scrollbar theme colors
    scrollbarThumbColor: "#5F5F5F",
    scrollbarTrackColor: "#2F2F2F"
}

// Fluent theme
export const darkTheme = createTheme({
    isInverted: true,
    components: {
        "DetailsList": {
            styles: {
                root: {
                    selectors: {
                        '.ms-DetailsRow:hover': {
                            backgroundColor: "unset",
                            background: hilightColor
                        }
                    }
                }
            }
        },
        "Modal": {
            styles: {
                main: {
                    background: hordeDarkTheme.contentBackground,
                }
            }
        },
        "Stack": {
            styles: {
                root: {
                    selectors: {
                        'a': {
                            color: linkColor
                        },
                        'a:hover': {
                            color: linkColorHovered
                        }
                    }
                }
            }
        }
    },
    palette: {
        themePrimary: '#0078d4',
        themeLighterAlt: '#eff6fc',
        themeLighter: '#deecf9',
        themeLight: '#c7e0f4',
        themeTertiary: '#71afe5',
        themeSecondary: '#2b88d8',
        themeDarkAlt: '#106ebe',
        themeDark: '#005a9e',
        themeDarker: '#004578',
        neutralLighterAlt: '#363636',
        neutralLighter: '#363636',
        neutralLight: '#363636',
        neutralQuaternaryAlt: '#363636',
        neutralQuaternary: '#363636',
        neutralTertiaryAlt: '#363636',
        neutralTertiary: '#c8c8c8',
        neutralSecondary: '#d0d0d0',
        neutralPrimaryAlt: '#dadada',
        neutralPrimary: '#ffffff',
        neutralDark: '#f4f4f4',
        black: '#f8f8f8',
        white: '#181818',
    },
    semanticColors: {
        bodyBackground: hordeDarkTheme.neutralBackground,
        bodyText: textColor,
        buttonText: textColor,
        actionLink: textColor,
        link: linkColor,
        linkHovered: linkColorHovered,
        menuHeader: "#55B7FF",
        menuItemText: textColor,
        menuItemTextHovered: "#F1F1F1",
        menuItemBackgroundHovered: hilightColor,
        listText: textColor,
        primaryButtonText: textColor,
        primaryButtonTextHovered: textColor

    },
    defaultFontStyle: {
        fontFamily: 'Horde Open Sans Regular'
    },
    fonts: {
        small: { fontSize: 12 },
        medium: { fontSize: 13 },
        mediumPlus: { fontSize: 18 },
        large: { fontSize: 32 }
    }
}) as HordeTheme;

darkTheme.horde = hordeDarkTheme;