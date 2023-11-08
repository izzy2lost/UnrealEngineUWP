# live_link_vcam

A rebuild of the Live Link VCAM app for Flutter, enabling cross-platform deployment.

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Lab: Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Cookbook: Useful Flutter samples](https://docs.flutter.dev/cookbook)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.

## Signing Certificates 

To setup signing certificates, follow the instructions outlined in this article 

 - [Deploying Flutter Apps to The PlayStore](https://medium.com/@bernes.dev/deploying-flutter-apps-to-the-playstore-1bd0cce0d15c)

Add your `android-key.tks` to `android\app` folder and `key.properities` to `android\`.  The gradle files are already setup
to read these files when app bundling

