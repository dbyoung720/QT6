// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

//! [QtQuick import]
import QtQuick
//! [QtQuick import]
import QtPositioning
import QtLocation

Item {
    width: 400; height: 400;
    Plugin {
        id: myPlugin
    }

    Place {
        id: place
    }

    //! [Category]
    Category {
        id: category

        plugin: myPlugin
        name: "New Category"
        visibility: Category.PrivateVisibility
    }
    //! [Category]

    function saveCategory() {
        //! [Category save]
        category.save();
        //! [Category save]
    }

    //! [CategoryView]
    ListView {
        model: CategoryModel {
            plugin: myPlugin
            hierarchical: false
        }
        delegate: Text { text: category.name }
    }
    //! [CategoryView]

    //! [ExtendedAttributes]
    ListView {
        model: place.extendedAttributes.keys()
        delegate: Text {
            text: "<b>" + place.extendedAttributes[modelData].label + ": </b>" +
                  place.extendedAttributes[modelData].text
        }
    }
    //! [ExtendedAttributes]

    //! [ExtendedAttributes read]
    function printExtendedAttributes(extendedAttributes) {
        var keys = extendedAttributes.keys();
        for (var i = 0; i < keys.length; ++i) {
            var key = keys[i];
            if (extendedAttributes[key].label !== "")
                console.log(extendedAttributes[key].label + ": " + extendedAttributes[key].text);
        }
    }
    //! [ExtendedAttributes read]

    function writeExtendedAttributes() {
    //! [ExtendedAttributes write]
        //assign a new attribute to a place
        var smokingAttrib = Qt.createQmlObject('import QtLocation; PlaceAttribute {}', place);
        smokingAttrib.label = "Smoking Allowed"
        smokingAttrib.text = "No"
        place.extendedAttributes.smoking = smokingAttrib;

        //modify an existing attribute
        place.extendedAttributes.smoking.text = "Yes"
    //! [ExtendedAttributes write]
    }

    Icon {
        id: icon
    }
    //! [Icon]
    Image {
        source: icon.url(Qt.size(64, 64))
    }
    //! [Icon]

    Image {
    //! [Icon default]
        source: icon.url()
    //! [Icon default]
    }

    //! [SearchSuggestionModel]
    PlaceSearchSuggestionModel {
        id: suggestionModel

        plugin: myPlugin

        // Brisbane
        searchArea: QtPositioning.circle(QtPositioning.coordinate(-27.46778, 153.02778))

        onSearchTermChanged: update()
    }

    ListView {
        model: suggestionModel
        delegate: Text { text: suggestion }
    }
    //! [SearchSuggestionModel]

    //! [EditorialModel]
    EditorialModel {
        id: editorialModel
        batchSize: 3
        place: place
    }

    ListView {
        model: editorialModel
        delegate: Item {
            anchors.fill: parent

            Column {
                width: parent.width
                clip: true

                Text {
                    text: title
                    width: parent.width
                    wrapMode: Text.WordWrap
                    font.pixelSize: 24
                }

                Text {
                    text: text
                    width: parent.width
                    wrapMode: Text.WordWrap
                    font.pixelSize: 20
                }

                Row {
                    Image {
                        width: 16
                        height: 16

                        source: supplier.icon.url(Qt.size(width, height), Icon.List)
                    }

                    Text {
                        text: "Provided by " + supplier.name
                        font.pixelSize: 16
                    }
                }

                Text {
                    text: "Contributed by " + user.name
                    font.pixelSize: 16
                }

                Text {
                    text: attribution
                    font.pixelSize: 8
                }
            }
        }
    }
    //! [EditorialModel]

    //! [ImageModel]
    ImageModel {
        id: imageModel
        batchSize: 3
        place: place
    }

    ListView {
        anchors.top: parent.top
        width: parent.width
        spacing: 10

        model: imageModel
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem

        delegate: Item {
            width: listView.width
            height: listView.height

            Image {
                anchors.fill: parent
                source: url
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: supplier.name + "\n" + supplier.url
                width: parent.width
                anchors.bottom: parent.bottom
            }
        }
    }
    //! [ImageModel]

    //! [Supplier]
    Supplier {
        id: placeSupplier
        name: "Example"
        url: "http://www.example.com/"
    }

    Text {
        text: "This place is was provided by " + placeSupplier.name + "\n" + placeSupplier.url
    }
    //! [Supplier]

    //! [Ratings]
    Text {
        text: "This place is rated " + place.ratings.average + " out of " + place.ratings.maximum + " stars."
    }
    //! [Ratings]

    //! [ContactDetails read]
    function printContactDetails(contactDetails) {
        var keys = contactDetails.keys();
        for (var i = 0; i < keys.length; ++i) {
            var contactList = contactDetails[keys[i]];
            for (var j = 0; j < contactList.length; ++j) {
                console.log(contactList[j].label + ": " + contactList[j].value);
            }
        }
    }
    //! [ContactDetails read]

    //! [ContactDetails write single]
    function writeSingle() {
        var phoneNumber = Qt.createQmlObject('import QtLocation; ContactDetail {}', place);
        phoneNumber.label = "Phone";
        phoneNumber.value = "555-5555"
        place.contactDetails.phone = phoneNumber;
    }
    //! [ContactDetails write single]

    //! [ContactDetails write multiple]
    function writeMultiple() {
        var bob = Qt.createQmlObject('import QtLocation; ContactDetail {}', place);
        bob.label = "Bob";
        bob.value = "555-5555"

        var alice = Qt.createQmlObject('import QtLocation; ContactDetail {}', place);
        alice.label = "Alice";
        alice.value = "555-8745"

        var numbers = new Array();
        numbers.push(bob);
        numbers.push(alice);

        place.contactDetails.phone = numbers;
    }
    //! [ContactDetails write multiple]

    //! [ContactDetails phoneList]
    ListView {
        model: place.contactDetails.phone;
        delegate: Text { text: modelData.label + ": " + modelData.value }
    }
    //! [ContactDetails phoneList]

    //! [Place savePlace def]
    Place {
        id: myPlace
        plugin: myPlugin

        name: "Brisbane Technology Park"
        location: Location {
            address: Address {
                street: "53 Brandl Street"
                city: "Eight Mile Plains"
                postalCode: "4113"
                country: "Australia"
            }
            coordinate {
                latitude: -27.579646
                longitude: 153.100308
            }
        }

        visibility: Place.PrivateVisibility
    }
    //! [Place savePlace def]

    function fetchDetails() {
        //! [Place fetchDetails]
        if (!place.detailsFetched)
            place.getDetails();
        //! [Place fetchDetails]
    }

    function savePlace() {
    //! [Place savePlace]
        myPlace.save();
    //! [Place savePlace]
    }

    function createAndSavePlace() {
        //! [Place createAndSavePlace]
        //creating and saving a place
        var place = Qt.createQmlObject('import QtLocation; Place { }', parent);
        place.plugin = myPlugin;
        place.name = "New York";
        place.location.coordinate.latitude = 40.7
        place.location.coordinate.longitude = -74.0
        place.save();
        //! [Place createAndSavePlace]
    }

    function removePlace() {
        //! [Place removePlace]
        //removing a place
        place.remove();
        //! [Place removePlace]
    }

    function saveToNewPlugin() {
        //! [Place save to different plugin]
        var place = Qt.createQmlObject('import QtLocation; Place { }', parent);
        place.plugin = destinationPlugin;
        place.copyFrom(originalPlace);
        place.save();
        //! [Place save to different plugin]
    }

    function getPlaceForId() {
    //! [Place placeId]
        place.plugin = myPlugin;
        place.placeId = "known-place-id";
        place.getDetails();
    //! [Place placeId]
    }

    function primaryContacts() {
    //! [Place primaryPhone]
        var primaryPhone;
        if (place.contactDetails["phone"].length > 0)
            primaryPhone = place.contactDetails["phone"][0].value;
    //! [Place primaryPhone]
    //! [Place primaryFax]
        var primaryFax;
        if (place.contactDetails["fax"].length > 0)
            primaryFax = place.contactDetails["fax"][0].value;
    //! [Place primaryFax]
    //! [Place primaryEmail]
        var primaryEmail;
        if (place.contactDetails["email"].length > 0)
            primaryEmail = place.contactDetails["email"][0].value;
    //! [Place primaryEmail]
    //! [Place primaryWebsite]
        var primaryWebsite;
        if (place.contactDetails["website"].length > 0)
            primaryWebsite = place.contactDetails["website"][0].value;
    //! [Place primaryWebsite]
    }

    //! [Place favorite]
    Text { text: place.favorite ? place.favorite.name : place.name }
    //! [Place favorite]

    function saveFavorite() {
        var place = Qt.createQmlObject('import QtLocation; Place { }', parent);
        var destinationPlugin
        //! [Place saveFavorite]
        place.initializeFavorite(destinationPlugin);
        //if necessary customizations to the favorite can be made here.
        //...
        place.favorite.save();
        //! [Place saveFavorite]
    }

    function removeFavorite() {
        var place;
        //! [Place removeFavorite 1]
        place.favorite.remove();
        //! [Place removeFavorite 1]

        //! [Place removeFavorite 2]
        //check successful removal of the favorite by monitoring its status.
        //once that is done we can assign null to the favorite
        place.favorite = null;
        //! [Place removeFavorite 2]
    }

    function connectStatusChangedHandler() {
        //! [Place checkStatus]
        place.statusChanged.connect(statusChangedHandler);
        //! [Place checkStatus]
    }

    //! [Place checkStatus handler]
    function statusChangedHandler() {
        if (statusChangedHandler.prevStatus === Place.Saving) {
            switch (place.status) {
            case Place.Ready:
                console.log('Save successful');
                break;
            case Place.Error:
                console.log('Save failed');
                break;
            default:
                break;
            }
        }
        statusChangedHandler.prevStatus = place.status;
    }
    //! [Place checkStatus handler]
}
